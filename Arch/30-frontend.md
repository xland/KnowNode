# 30 · 前端架构（UI）

## 代码位置

- 大部分 UI 相关代码在 **`UI/src/`**。
- 每个组件一个目录，内含同名三件套：`Xxx.html` + `Xxx.scss` + `Xxx.ts`。

## 与 C++ 的通信

- 唯一通道：**`UI/src/Msg.ts`**。前端所有跨语言调用都经过它，不在别处直接碰 `window.chrome.webview`。
- 底层是 WebView2 的 `window.chrome.webview.postMessage` / `addEventListener("message")`。

`Msg.ts` 提供的能力：

| API | 用途 |
| --- | --- |
| `invoke(method, args)` | 调用原生方法，返回 Promise（请求-响应，带自增 id 配对） |
| `invokeWithObjects(method, args)` | 同上，但 resolve `{ result, objects }`；`objects` 是原生用 `PostWebMessageAsJsonWithAdditionalObjects` 附带的对象（如 File System Access 目录句柄） |
| `on / off / once / emit` | 事件订阅与派发：**既收原生推送的事件，也承载前端内部的事件**，两类共用这一条通道 |

> 协议形态：请求 `{ id, method, args }` → 响应 `{ id, result | error }`；事件 `{ eventName, ... }`。
> 事件**没有 `id`、没有回包**，是单向广播。

### 事件清单（2026-10-08）

`Msg` 这一条通道上跑着**两类事件**，写法一样但**来源不同**，别混为一谈：

| eventName | 来源 | 发出方 | 订阅方 |
| --- | --- | --- | --- |
| `maximize` | **原生推送** | C++ `Window::wndProc` → `page->emit()` | `WindowBorder.ts`、`TitleBar.ts` |
| `restore` | **原生推送** | 同上 | 同上 |
| `editorState` | 前端内部 | `EditorContent/EditorPlugin.ts` | `EditorBar/` 各按钮、`ToolbarButton.ts` |
| `editorContentChanged` | 前端内部 | `EditorPlugin.ts`、`EditorBar/Code/Code.ts` | `KnowDetail.ts`（防抖保存） |
| `editorBlur` | 前端内部 | `EditorContent.ts`（`focusout`） | `KnowDetail.ts`（flush） |
| `editCodeBlock` | 前端内部 | `EditorContent.ts` | `EditorBar/Code/Code.ts` |
| `knowDetailExpanded` | 前端内部 | `KnowDetail.ts` | `ContentBox.ts`（重排 splitter 与把手按钮） |

- **只有 `maximize` / `restore` 是 C++ 发出来的**，其余都在前端自己 `Msg.emit()` / `Msg.on()`，不跨语言。
- 原生事件在 [21-ipc.md](./21-ipc.md)「事件推送」登记；method 清单也在那里。

## 界面布局

界面分**上中下三个大组件**：

| 区域 | 组件 | 路径 |
| --- | --- | --- |
| 标题栏 | `TitleBar` | `UI/src/TitleBar/` |
| 容器区（主体） | `ContentBox` | `UI/src/ContentBox/` |
| 状态栏 | `StatusBar` | `UI/src/StatusBar/` |

**额外组件**（不属于上中下三区）：

| 组件 | 路径 | 职责 |
| --- | --- | --- |
| `WindowBorder` | `UI/src/WindowBorder/` | 窗口边框，**用于拖拽改变窗口大小**（无边框窗口的自绘 resize 热区）；**最大化时集体失效** |

### WindowBorder：最大化时失效（**2026-10-07 定**）

- 8 个触发区（四条边 + 四个角）只在**非最大化**时可用：最大化时窗口拖不动边框，
  留着它们只会让光标在边缘变成调整大小的样式、点了却没反应。
- 失效靠两道：CSS 摘掉 `pointer-events`（鼠标不再命中、光标也不再变），
  `mousedown` 里再判断一次（免得将来改样式又把拖拽放出来）。
- 状态来自 C++ 的 WM_SIZE 广播（`maximize` / `restore` 事件，与 `TitleBar` 那两个按钮同源）。
  **初值按"已最大化"**：C++ 侧 `Window::show()` 用的是 `SW_SHOWMAXIMIZED`，一启动就是最大化。
- C++ 的 `Window::hittest()` 再兜一道：命中值落在 `HTLEFT(10)..HTBOTTOMRIGHT(17)` 且 `IsZoomed`
  时直接返回（`HTCAPTION` 拖动标题栏不受影响）。

## TitleBar 与 StatusBar（已确定）

### TitleBar

- **保留现有窗口控制能力**：最小化 / 最大化 / 关闭按钮，以及**拖拽移动窗口**。
- 与 `WindowBorder` 配合（无边框窗口自绘标题栏 + 自绘 resize 热区）。

### StatusBar

| 位置 | 显示内容 |
| --- | --- |
| **左侧** | **当前选中的是哪个节点** |
| **右侧** | **一共有多少个节点**（当前知识的节点总数） |

> 待确认：未选中任何节点时左侧显示什么（如「未选中」或留空）；节点总数是否随增删实时更新。

## 其他现存目录

`UI/src/` 下还有：

| 目录 / 文件 | 状态 |
| --- | --- |
| `EditorBar/`、`EditorContent/` | **保留**：roosterjs 富文本编辑器的工具条与编辑区，用于知识详情（见 33） |
| `Dialog/`、`Menu/` | **新增**：通用悬浮对话框（新建 / 重命名知识）与通用右键菜单（列表项与画布共用），
  都挂 `body`——面板多有 `overflow: hidden`，留在面板里会被裁掉 |
| `ArticleEditor/`、`ArticleTitle/`、`EditorTitle/` | **已删除**（旧项目「文章」相关，见 [20](./20-data-layer.md)） |
| `CodeHighlight.ts`、`CtrlBase.ts`、`ImageStore.ts`、`Main.scss`、`Main.ts`、`ToolbarButton.ts` | 用途待梳理 |

## 已知问题（**用户已批准修复**）

- `Msg.ts` 中 `cache: Map<String, any>` **同时**存放「请求 id → pending」和「事件名 → 监听器数组」，
  两类数据共用一个 Map。**已确认修复**：拆成两个 Map（pending 一个、事件监听器一个）。

## 待确认问题

- ~~`CodeHighlight.ts`、`CtrlBase.ts`、`ImageStore.ts`、`ToolbarButton.ts` 等散件的用途梳理~~
  —— **已明确（2026-10-08）**：`CtrlBase.ts` = 组件抽象基类；`ToolbarButton.ts` = 工具条按钮的
  声明式封装（`createButton` + `ToolbarButton` 接口）；`CodeHighlight.ts` = shiki 代码高亮
  （`highlightCode` / `CODE_LANGS`，给代码块用）；`ImageStore.ts` = 图片落盘（见 [33](./33-know-detail.md)）。
- **未选中节点时状态栏左侧显示「未选中节点」是否合意**（已先按这个实现）—— 见 README F3。

## 已实现（2026-10-06）

- `Msg.ts` 拆成两个 Map（pending / 事件监听器）。
- 旧文章组件删除，`ContentBox` 改为挂 `KnowList` / `KnowNet` / `KnowDetail`；splitter 机制泛化。
- `StatusBar` 按新定义重写：左侧当前选中节点、右侧当前知识节点总数。
- `TitleBar` / `WindowBorder` 的窗口 method 已改为 `win.*`；`Main.ts` 的 `showWindow` → `win.show`。
