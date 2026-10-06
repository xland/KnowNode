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
| `on / off / once / emit` | 事件订阅与派发，用于原生主动推送的事件（`msg.eventName`） |

> 协议形态：请求 `{ id, method, args }` → 响应 `{ id, result | error }`；事件 `{ eventName, ... }`。

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
| `WindowBorder` | `UI/src/WindowBorder/` | 窗口边框，**用于拖拽改变窗口大小**（无边框窗口的自绘 resize 热区） |

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

- `CodeHighlight.ts`、`CtrlBase.ts`、`ImageStore.ts`、`ToolbarButton.ts` 等散件的用途梳理
  （`ImageStore.ts` 已确认保留，见 [33](./33-know-detail.md)）。
- 未选中节点时状态栏左侧显示「未选中节点」是否合意（已先按这个实现）。

## 已实现（2026-10-06）

- `Msg.ts` 拆成两个 Map（pending / 事件监听器）。
- 旧文章组件删除，`ContentBox` 改为挂 `KnowList` / `KnowNet` / `KnowDetail`；splitter 机制泛化。
- `StatusBar` 按新定义重写：左侧当前选中节点、右侧当前知识节点总数。
- `TitleBar` / `WindowBorder` 的窗口 method 已改为 `win.*`；`Main.ts` 的 `showWindow` → `win.show`。
