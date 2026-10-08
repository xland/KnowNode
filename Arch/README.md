# KnowNode · 知点

> 本目录是本项目的**唯一事实来源（source of truth）**：实现代码时以本目录文档为准；
> 文档未覆盖或与代码冲突处，先与用户确认再动手。
>
> **2026-10-08 更正（重要）**：此前把 `KnowNode/` C++ 工程写成"老项目残留，不作为设计依据"，
> **这个说法是错的**——它就是**本项目的现行实现**（宿主窗口、WebView2 页面与消息分发、
> SQLite 数据层全部在里面）。仓库各目录的真实身份：
>
> | 目录 | 身份 |
> | --- | --- |
> | `KnowNode/` | **现行 C++ 实现**（`main.cpp`、`Window`、`Page`、`Db/` 五个表类），**不是残留** |
> | `packages/` | 第三方依赖（WebView2 等），用户已配置好，**不要清理** |
> | `x64/`、`KnowNode/x64/` | **构建产物**（当前构建的输出目录），不是残留 |
>
> 因此"文档与代码不符"不等于"代码是残留"：冲突时按上面的原则先确认再动手。

## 一句话定位

**节点化个人知识管理工具。**

## 文档索引

| 文档 | 内容 | 状态 |
| --- | --- | --- |
| [10-overview.md](./10-overview.md) | 项目定位、核心理念 | 已记录 |
| [11-platform.md](./11-platform.md) | 平台（WebView2 / Windows）、技术栈、目录结构 | 已记录 |
| [20-data-layer.md](./20-data-layer.md) | 数据层：SQLite、`Db` 管理类、一表一类约定 | 已记录 |
| [21-ipc.md](./21-ipc.md) | C++ ↔ 前端通信：协议、`msgHandlers()` 注册表、事件清单 | 已记录 |
| [30-frontend.md](./30-frontend.md) | 前端：`Msg.ts` 通信、三大布局组件、`WindowBorder`、事件清单 | 已记录 |
| [31-content-box.md](./31-content-box.md) | 容器区：KnowList / KnowNet / KnowDetail，splitter 规则 | 已记录（悬浮 / 钉住已取消） |
| [32-know-net.md](./32-know-net.md) | KnowNet 画布：节点、连线、手动拖拽摆放、缩放、框选与多选 | 已记录（力导向 / 一键整理 / 权重已删除） |
| [33-know-detail.md](./33-know-detail.md) | KnowDetail：知识本身 / 节点 / 连线三者的富文本详情 | 已记录（标题输入框已移除，改标题只有画布一个入口） |
| [34-know-list.md](./34-know-list.md) | KnowList 面板：新建/重命名/删除知识 | 已记录 |
| [40-data-model.md](./40-data-model.md) | 数据模型：know_list / know_node / know_line / know_detail / setting | 已记录（**SQL 已落地**，不做老库兼容） |

## 待确认问题汇总

> 各文档末尾还有各自的待确认清单，本节只汇总**跨文档 / 阻塞实现**的项。
> **状态：2026-10-08 全量刷新**——逐条对着代码核过：已定的打 ✅ 并标出落地处，
> 改定 / 作废的划掉并说明，**真正未定的全部收在 [F](#f-真正未定只剩这些) 一节**。

### A. 数据层与旧代码清理 —— **全部已确定**

1. ✅ `detail_id` **非空 + 外键默认 `RESTRICT`**；节点/连线创建即建一条空详情行，前端不处理"没有详情"的分支。
2. ✅ 详情行**应用层删除**（表类 `remove()` 里先记下 `detail_id`，删主体后逐个删详情），不引触发器。
3. ✅ **`Db` 自注册方案认可**（`Db::instance()` 函数内静态，避静态初始化顺序问题）。
4. ✅ 旧代码清理：文章相关（`ArticleEditor` / `ArticleTitle` / `EditorTitle`、`category` / `article` 表）**删除**；
   **图片链条保留**（`ImageStore.ts`、`ImagePlugin.ts`、`EditorBar/Image/`、
   `Page::handleGetImageDir`、`images` 目录），仅改 method 名；
   `packages/` 不动。
   > 2026-10-08 更正：上句原列在"保留"里的三项**实际并不存在**，已从清单删掉——
   > `ImageResize.ts`、`Page::handleResizeImage`（图片不再另存缩放图）、
   > **老 `image` 表**（数据层只有 `know_list` / `know_node` / `know_line` / `know_detail` / `setting` 五张表，
   > 没有任何表记录图片引用）。
5. ✅ **不做老库兼容**（2026-10-08 定）：项目尚未发布、没有既有用户数据，**不支持已有的 `db.db`**。
   列一律写在 `CREATE TABLE` 里，`Db::ensureColumn()` 与 `KnowList::ensureDetail()` **已删除**。
   见 [20](./20-data-layer.md)「已确定的设计决策」5。

> 以上 1–3 已在 `KnowNode/Db/` 落地；4 已清理完；5 已删除落地（2026-10-08）。

### B. 前端骨架 —— **全部已确定**

6. ✅ **三栏宽度**：`KnowList` 最小 300、**`KnowNet` 最小 500**、`KnowDetail` **默认 600 / 最小 300**
   （`ContentBox.ts` 的 `KNOW_LIST_MIN` / `KNOW_NET_MIN` / `KNOW_DETAIL_MIN`
   与 `KnowDetail.scss` 的 `width` / `min-width`）。
7. ✅ ~~悬浮态 `KnowDetail` 允许拖宽~~ → **改定（2026-10-07）：没有悬浮态了**。
   详情栏就是三栏里的普通一栏，**拖它左边缘那条 splitter 调宽**，宽度存 `setting` 的 `know_detail.width`。
8. ✅ ~~钉住 / 取消钉住的钉子按钮~~ → **已作废（2026-10-07）**：详情栏不悬浮、不钉住，
   `know_detail.pinned` 这个设置项一并取消，面板顶部标题栏也去掉了。
   - ✅ `KnowDetail` 的**最小宽度 = 300px**（不再是"暂取"）。

### C. KnowNet 细节 —— **全部已确定（不再阻塞）**

9. ✅ **节点尺寸**（`KnowNet.ts` 顶部常量）：高 36、字号 13、圆角 6，宽度按文字自适应并夹在 **[72, 220]**。
10. ✅ **缩放**：范围 `[0.2, 3]`，Ctrl + 滚轮每格 ×1.08，**以鼠标位置为锚点**；不按 Ctrl 的滚轮是上下挪画布。
    （上下限是**先取的一组值，待实测微调**——属调参，不阻塞，见 [F](#f-真正未定只剩这些)。）
11. ✅ **双击改标题**：**覆盖的 DOM `<input>`**（不是 Konva 内嵌）；回车 / 失焦 = 提交（空串回「未命名」），
    Esc = 放弃；改标题时按 Tab 可连打下一个。
12. ✅ **拖连线 vs 拖移动**：矩形**边缘 6px** 是拉线热区，中间才是拖节点；拉线时画蓝色虚线预览、
    指针下的节点高亮；落在空白 / 拖回自己 / 已连过 → **无声取消**。
13. ✅ **`node.move` 的写入时机**：**拖拽结束写一次**（`dragend`），过程中不写；多选时整批逐个写。
14. ✅ **空白处右键新建节点的坐标**：**建在右键处**（屏幕坐标换算成画布坐标），不是视口中心。
15. ✅ **多选 / 框选：支持**（2026-10-07 加）——拖空白拉框，交叠即选中；Ctrl + 单击加选 / 取消；
    可整批移动、整批删除。

### D. KnowDetail / KnowList 细节 —— **全部已确定（不再阻塞）**

16. ✅ ~~标题两处编辑的同步~~ → **问题已消除**：详情面板里的标题输入框已移除（2026-10-07），
    改标题**只有画布双击这一个入口**，不存在两份需要互相同步的地方。
17. ✅ **防抖 flush 时机**（间隔 `SAVE_DELAY = 300ms`）——下列时机全都做了：
    **切换选中对象**（`switchTo`）、**收起 / 关面板**（`setOpen(false)`、`close`）、
    **切知识**（走 `switchTo`）、**编辑器失焦**（`editorBlur`）、
    **关窗口**（`TitleBar.onClose()` 先 `await KnowDetail.flush()` 再 `window.close()`）。
18. ✅ **详情中的图片**：存数据目录的 **`images`** 子目录；前端 `ImageStore.ts` 用原生给的目录句柄
    直接在 JS 侧写文件，正文存 `https://app.localhost/images/<文件名>`；**没有 `image` 表**。
19. ✅ **KnowList**：
    - 新建对话框的取消方式：取消按钮 / Esc / 点面板外 / 再点一次加号，**没有遮罩**；
    - 名称校验：**长度上限 36（已落地）**，空 / 纯空白不建，**重名不校验**；
    - 删除**不做二次确认**；删的正是当前打开的知识就改选第一行，一行不剩则清空画布；
    - 「修改知识名称」复用同一个悬浮 `Dialog`；右键菜单只有「修改 / 删除」两项；
    - 排序：**按创建时间升序**（`ORDER BY created_at, id`）——**是否支持拖动排序仍未定**，见 F。

### E. 文档自身的矛盾与过时 —— **已全部回改（2026-10-08）**

本轮对着代码逐条核过并回改的项：

| 文档 | 原来的错 | 现状 |
| --- | --- | --- |
| `README` 头部 | 把 `KnowNode/` 写成"老项目残留，不作为设计依据" | 它是**现行实现**；已改成目录身份表 |
| [10](./10-overview.md)、[11](./11-platform.md) | 同上，"不属于新架构 / 哪些是残留待确认" | 已更正并标注已定 |
| [20](./20-data-layer.md) | `Db` 是 `namespace Db` 自由函数、`createSchema()` 为空 | 已是单例**类**，`createSchema()` 不存在 |
| [20](./20-data-layer.md)、[21](./21-ipc.md)、[33](./33-know-detail.md) | 「老 `image` 表保留」「`migrateImageTable()` 保留」 | **都没有**，库里只有五张表 |
| [21](./21-ipc.md) | 「没有任何 C++ 主动推送的事件，清单为空」 | 有 `maximize` / `restore`，清单已列 |
| [21](./21-ipc.md) | 异步靠后台线程 + `WM_DD_POST_JSON` + `handleResizeImage` | 三者都不存在，现状**全同步** |
| [21](./21-ipc.md) | 「现有 method（老项目残留）」列 `showWindow` 等旧名 | 旧名全无，method 只在 `msgHandlers()` 注册表 |
| [30](./30-frontend.md) | `Msg.on` 只用于原生推送事件 | 已分「原生推送 / 前端内部」两类并列表 |
| [31](./31-content-box.md) | `ContentBox.ts` 仍 import 并挂载 `ArticleTitle` / `ArticleEditor` | 已删净，只挂三个 `Know*` |
| [32](./32-know-net.md) | 「新建知识时画布已有一个『未命名』节点」 | **不建节点**，画布是空的 |
| [33](./33-know-detail.md) | 详情面板默认 500px | **600px**（`KnowDetail.scss`） |
| [33](./33-know-detail.md)、[34](./34-know-list.md) | 36 字符上限"落在画布输入框里"、常量集中一处 | 只**知识名称**落地；画布输入框**没有**校验 |
| [40](./40-data-model.md) | 后加的列靠 `ensureColumn` 补、`ensureDetail` 兜底 | **不做老库兼容**，两者已删除 |

早先已解决、此处一并留档：

- ✅ [40](./40-data-model.md)：`detail_id` 空/非空两处不一致 —— 已改为非空（A1）。
- ✅ [20](./20-data-layer.md)：类名不一致 —— 已统一为 `KnowNode` / `DbTableRegistrar<KnowNode>`。
- ✅ 聚合 method「一次取回某知识全部数据」—— **已有**：`list.open`。

### F. 真正未定（只剩这些）

| # | 未定项 | 现状 |
| --- | --- | --- |
| F1 | **缩放上下限的实测微调** | 先取 `[0.2, 3]`，用到再调（[32](./32-know-net.md)） |
| F2 | **知识列表是否支持拖动排序** | 现在是按创建时间升序，没做排序交互（[34](./34-know-list.md)） |
| F3 | **未选中节点时状态栏左侧的文案** | 已先按「未选中节点」实现，是否合意待你确认（[30](./30-frontend.md)） |
| F4 | **节点标题的 36 字符上限要不要补** | 现在只有知识名称有；画布那个输入框没有校验（[33](./33-know-detail.md)） |
| F5 | **是否要做撤销（undo）** | 未做；目前删错只能"再建一个"（[32](./32-know-net.md)） |

> 散件用途（[30](./30-frontend.md) 曾标"待梳理"）已明确：`CtrlBase.ts` = 组件抽象基类、
> `ToolbarButton.ts` = 工具条按钮的声明式封装、`CodeHighlight.ts` = shiki 代码高亮、
> `ImageStore.ts` = 图片落盘。不算未定项。
