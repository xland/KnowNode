# 21 · C++ ↔ 前端 通信（IPC）

## 通道

| 侧 | 入口 |
| --- | --- |
| 前端 | `UI/src/Msg.ts`（唯一通道），底层 `window.chrome.webview` |
| C++ | **`KnowNode/Page.cpp` 的 `Page::onMsgReceived`**（`add_WebMessageReceived` 注册） |

## 协议

**请求**（前端 → C++）：

```json
{ "id": "随机id", "method": "方法名", "args": { ... } }
```

- `args` 是**嵌套对象**，C++ 侧用 `param.GetNamedObject(L"args")` 取，再取其中的字段。

**响应**（C++ → 前端）：

```json
{ "id": "...", "result": ... }        // 成功
{ "id": "...", "error": "..." }       // 失败，前端 Msg.invoke 会 reject
```

- C++ 侧用 Windows Runtime 的 `JsonObject` / `JsonValue` 构造，`webview->PostWebMessageAsJson` 发出。
- **必须回带 `id`**，前端靠它配对 Promise。
- **未知 method 必须回 `error`**（现有代码已如此），否则前端会静默 `resolve(undefined)`，
  出现"原生改了没重新编译 exe"这类问题时难以排查。新增业务方法时在 `onMsgReceived` 里加分支即可。

**事件推送**（C++ 主动 → 前端）：`Page::emit(const JsonObject&)` → `PostWebMessageAsJson`，
前端用 `Msg.on(eventName, ...)` 接收（对应 `Msg.ts` 里 `msg.eventName` 分支）。

## 现有 method（示例 / 老项目残留）

| method | 作用 | 状态 |
| --- | --- | --- |
| `showWindow` | 显示窗口 | 窗口控制 |
| `hittest` | 前端 `WindowBorder` 命中的 `HT_*` 值传给原生做拖拽/改变窗口大小 | 窗口控制 |
| `minimize` / `maximize` / `restore` | 最小化 / 最大化 / 还原 | 窗口控制 |
| `getImageDir` | 老项目的图片目录句柄（随回包附带对象） | **老项目残留，待清理** |

## 异步处理约定

- 耗时操作**不要**在消息回调里同步做（会卡界面）。
- 现有做法：后台线程处理，完成后投递 **`WM_DD_POST_JSON`** 回 UI 线程再发回包（见 `handleResizeImage` 的说明）。
- 数据库读写若耗时较长，应遵循同一模式。

## 前端产物如何进入 exe（已从代码确认）

- **Debug 构建**：`webview->Navigate(L"http://localhost:5173")` —— 连 vite 开发服务器，改前端不必重编 exe，并自动打开 DevTools。
- **Release 构建**：`Navigate(L"https://app.localhost/index.html")` —— 前端 `dist` 编进 exe 资源
  （见 `Resource.rc` 里的 dist 清单），请求由 `Page::onRequest` 从资源应答，一个 exe 独立运行。

## method 命名规范（**已确定**）

统一为**两段式**：`对象.动作`，全小写，不用 `know.` 前缀。

```
list.create    node.move    line.remove    detail.save    win.minimize
  │       │
  │       └── 动作：create / update / remove / get / list / move ...
  └────────── 对象：list / node / line / detail / setting / win
```

### 完整 method 清单

| method | args | 说明 |
| --- | --- | --- |
| `list.list` | – | 取所有知识 |
| `list.create` | `{ name }` | 新建知识 |
| `list.update` | `{ id, name }` | 重命名知识 |
| `list.remove` | `{ id }` | 删除知识（连带节点 / 连线 / 详情） |
| `node.list` | `{ listId }` | 取该知识的全部节点 |
| `node.create` | `{ listId, x, y }` | 新建节点（标题默认「未命名」） |
| `node.update` | `{ id, title }` | 改节点标题 |
| `node.move` | `{ id, x, y }` | 拖拽后保存坐标 |
| `node.remove` | `{ id }` | 删除节点（连带删其连线与详情） |
| `line.list` | `{ listId }` | 取该知识的全部连线 |
| `line.create` | `{ nodeAId, nodeBId }` | 建连线（校验两端同属一个知识） |
| `line.remove` | `{ id }` | 删除连线 |
| `detail.get` | `{ target, id }` | 取详情，`target` = `node` / `line` |
| `detail.save` | `{ target, id, content }` | 保存详情（前端防抖后调用） |
| `setting.get` | `{ key }` | 读设置项 |
| `setting.set` | `{ key, value }` | 写设置项 |

### 窗口类方法统一加 `win.` 前缀（**已确定改名**）

| 现名 | 改为 |
| --- | --- |
| `showWindow` | `win.show` |
| `hittest` | `win.hittest` |
| `minimize` | `win.minimize` |
| `maximize` | `win.maximize` |
| `restore` | `win.restore` |

> **连带改动**：前端 `TitleBar` / `WindowBorder` 中调用这些 method 的地方要同步改名。
> `getImageDir`（老项目图片相关）随文章功能一并删除。

### C++ 侧：method → handler 注册表（**已确定**）

`onMsgReceived` 不再堆 `if/else`，改为**注册表分发**：

- 注册方式沿用与数据层一致的自注册套路（表类自注册的同一风格）；
- 未命中的 method 仍然**回 `error`**（保留现有行为，便于暴露"原生没重编"的问题）；
- handler 统一接收 `args`，统一回包（自带回包的异步场景除外，如需要附带对象的调用）。
- 老项目图片相关（`getImageDir`、`handleResizeImage`、`Db/Image.h`）是否随文章功能一并删除。
