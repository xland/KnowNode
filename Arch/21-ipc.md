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

## method 现在都在哪

> **本节原来列的那张「老项目残留」表已删除（2026-10-08）**：`showWindow` / `hittest` /
> `minimize` / `maximize` / `restore` / `getImageDir` 这批**旧名一个都不存在了**，
> 全部已改名。留着它只会和下面的新命名清单打架。

- **唯一实现处**：`KnowNode/Page.cpp` 的 `msgHandlers()` 注册表（`std::map<method, handler>`），
  `onMsgReceived` 查表分发，查不到就回 `error`。**加 method 就是往这张表里加一项**，没有别的入口。
- 名字一律用下面的**两段式 `对象.动作`** 清单；窗口类是 `win.show` / `win.hittest` /
  `win.minimize` / `win.maximize` / `win.restore`，图片是 `image.dir`。

## 异步处理约定

> **本节原内容已作废（2026-10-08 核对）**：原文写"现有做法：后台线程处理，完成后投递
> **`WM_DD_POST_JSON`** 回 UI 线程再发回包（见 `handleResizeImage` 的说明）"——
> **这三样东西一个都不存在**：整个 C++ 侧没有 `std::thread` / `std::async` / `CreateThread`，
> 没有 `WM_DD_POST_JSON` 这条消息，`handleResizeImage` 也已删除。
> **别照着这一句去找代码。**

**现状：所有 method 都是同步的**——handler 在 WebView2 消息回调所在的 UI 线程上跑完，
直接 `PostWebMessageAsJson` 回包；SQLite 读写也在同一线程同步完成，没有跨线程回包的基础设施。

- 现在没有哪个 method 慢到需要异步：都是单条 SQL 的小读写，量级是个人知识工具的量级。
- **将来真出现耗时操作时再引异步**，届时两条硬约束：
  1. **必须回 UI 线程才能碰 webview**（工作线程上发回包不安全）；
  2. 回包**必须带原请求的 `id`**（前端靠它配对 Promise，丢了就永远 pending）。
- `onMsgReceived` 里唯一"不统一回包"的例外是 `image.dir`：它自己发过回包了
  （要用 `PostWebMessageAsJsonWithAdditionalObjects` 附带目录句柄），handler 返回 `true` 挡掉统一回包。

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
| `list.open` | `{ id }` | **聚合**：一次取回该知识的全部**节点 + 连线**（点列表项时用，省一次往返） |
| `list.create` | `{ name }` | 新建知识 |
| `list.update` | `{ id, name }` | 重命名知识 |
| `list.remove` | `{ id }` | 删除知识（连带节点 / 连线 / 详情） |
| `node.list` | `{ listId }` | 取该知识的全部节点 |
| `node.create` | `{ listId, x, y }` | 新建节点（标题默认「未命名」） |
| `node.update` | `{ id, title }` | 改节点标题 |
| `node.move` | `{ id, x, y }` | 拖拽后保存坐标 |
| `node.remove` | `{ id }` | 删除节点（连带删其连线与详情） |
| `node.color` | `{ id, color }` | 改节点的标记色（0 = 未着色，1..6） |
| `line.list` | `{ listId }` | 取该知识的全部连线 |
| `line.create` | `{ nodeAId, nodeBId }` | 建连线（校验两端同属一个知识） |
| `line.remove` | `{ id }` | 删除连线 |
| `line.color` | `{ id, color }` | 改连线的标记色（0 = 未着色，1..6） |
| `detail.get` | `{ target, id }` | 取详情，`target` = `list` / `node` / `line`（2026-10-07 起知识本身也有详情） |
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

### 图片相关 method（**已确定：保留，仅改名**）

图片逻辑与老项目一致，只按新命名规范改名（详见 [33-know-detail.md](./33-know-detail.md)）：

| 现名 | 改为 | 说明 |
| --- | --- | --- |
| `getImageDir` | `image.dir` | 取数据目录下 `images` 子目录的句柄，用 `PostWebMessageAsJsonWithAdditionalObjects` 附带对象 |
| ~~`resizeImage`~~ | ~~`image.resize`~~ | **已取消**：图片不再另存缩放图，改尺寸只改显示尺寸（见 [33](./33-know-detail.md)） |

> 前端 `ImageStore.ts` 里写死的 method 名已同步；`Page::handleGetImageDir` **保留不动**
> （**没有 `image` 表**——数据层只有 `know_list` / `know_node` / `know_line` / `know_detail` /
> `setting` 五张表，图片只落在 `images` 目录，2026-10-08 核对）；
> `Page::handleResizeImage`（本就只有声明没有实现）与前端 `ImageResizePlugin` **已删除**。

### 事件推送（C++ → 前端）

> **原文"目前没有任何 C++ 主动推送的事件，事件名清单为空"是错的（2026-10-08 更正）**：
> `Window.cpp` 的 `WM_SIZE` 分支里就在 `page->emit()`，前端 `WindowBorder.ts` / `TitleBar.ts`
> 也确实在用 `Msg.on` 订阅。清单如下。

| eventName | 由谁发 | 时机 | 前端订阅方 |
| --- | --- | --- | --- |
| `maximize` | C++ `Window::wndProc`（`wParam == SIZE_MAXIMIZED` → `page->emit`） | 窗口进入最大化 | `WindowBorder.ts`（停用 8 个 resize 热区）、`TitleBar.ts`（切按钮图标） |
| `restore` | C++ `Window::wndProc`（`wParam == SIZE_RESTORED` → `page->emit`） | 窗口退出最大化 | 同上，反向恢复 |

- 事件就是一发 `JsonObject`，**靠 `eventName` 字段区分**：`Msg.ts` 里 `msg.eventName` 有值就走
  `emit(eventName, msg)`，与「请求-响应」那条路（靠 `id` 配对）分开。
- **事件没有 `id`、没有回包**，是单向广播。
- 新增原生事件：`Window` / `Page` 里 `emit()`，并**登记到本表**；前端 `Msg.on(eventName, ...)` 订阅。
- 另有一批**前端内部事件**走的是同一条 `Msg` 通道（自己 `emit`、自己 `on`，不跨语言），
  清单见 [30-frontend.md](./30-frontend.md)「事件清单」——不要把两类混为一谈。

### C++ 侧：method → handler 注册表（**已确定**）

`onMsgReceived` 不再堆 `if/else`，改为**注册表分发**：

- 注册方式沿用与数据层一致的自注册套路（表类自注册的同一风格）；
- 未命中的 method 仍然**回 `error`**（保留现有行为，便于暴露"原生没重编"的问题）；
- handler 统一接收 `args`，统一回包（自带回包的异步场景除外，如需要附带对象的调用）。
- 图片相关**确定保留**的只有 `Page::handleGetImageDir`（method 名 `image.dir`）与 `images` 目录；
  `Page::handleResizeImage` 与老 `image` 表都**不存在**。文章相关才删。
