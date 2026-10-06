# KnowNode · 知点

> 本目录是本项目的**唯一事实来源（source of truth）**。
> 项目由老项目迁移而来，仓库中残留的旧代码（`KnowNode/` C++ 工程、`packages/`、`x64/` 等）不作为设计依据。
> 实现代码时以本目录文档为准；文档未覆盖或与旧代码冲突处，先与用户确认再动手。

## 一句话定位

**节点化个人知识管理工具。**

## 文档索引

| 文档 | 内容 | 状态 |
| --- | --- | --- |
| [10-overview.md](./10-overview.md) | 项目定位、核心理念 | 已记录 |
| [11-platform.md](./11-platform.md) | 平台（WebView2 / Windows）、技术栈、目录结构 | 已记录 |
| [20-data-layer.md](./20-data-layer.md) | 数据层：SQLite、`Db` 管理类、一表一类约定 | 已记录 |
| [21-ipc.md](./21-ipc.md) | C++ ↔ 前端通信协议（`Page::onMsgReceived`）、产物加载 | 已记录 |
| [30-frontend.md](./30-frontend.md) | 前端：`Msg.ts` 通信、三大布局组件、`WindowBorder` | 已记录 |
| [31-content-box.md](./31-content-box.md) | 容器区：KnowList / KnowNet / KnowDetail，splitter 规则 | 已记录 |
| [32-know-net.md](./32-know-net.md) | KnowNet 画布：节点、连线、手动拖拽摆放、缩放、一键整理 | 已记录（力导向仅作为按钮触发的一次性整理） |
| [33-know-detail.md](./33-know-detail.md) | KnowDetail：节点（标题+详情）/ 连线（仅详情） | 已记录（权重已删除） |
| [34-know-list.md](./34-know-list.md) | KnowList 面板：新建/重命名/删除知识 | 已记录 |
| [40-data-model.md](./40-data-model.md) | 数据模型：know_list / know_node / know_line / know_detail / setting | 已记录（命名已定，SQL 草案待落地） |

## 待确认问题汇总

> 各文档末尾还有各自的待确认清单，本节只汇总**跨文档 / 阻塞实现**的项。
> 状态：截至 2026-10-06 回顾。

### A. 数据层与旧代码清理 —— **已确定（2026-10-06）**

1. ✅ `detail_id` **非空 + 外键默认 `RESTRICT`**；节点/连线创建即建一条空详情行，前端不处理"没有详情"的分支。
2. ✅ 详情行**应用层删除**（表类 `remove()` 里先记下 `detail_id`，删主体后逐个删详情），不引触发器。
3. ✅ **`Db` 自注册方案认可**（`Db::instance()` 函数内静态，避静态初始化顺序问题）。
4. ✅ 旧代码清理：文章相关（`ArticleEditor` / `ArticleTitle` / `EditorTitle`、`category` / `article` 表）**删除**；
   **图片链条保留**（`ImageStore.ts`、`ImagePlugin.ts`、`ImageResize.ts`、`EditorBar/Image/`、
   `Page::handleGetImageDir`、`Page::handleResizeImage`、老 `image` 表），仅改 method 名；
   `packages/` 不动。

> 以上 1–3 已在 `KnowNode/Db/` 落地（经核对与决策一致）。

### B. 前端骨架 —— **基本已确定（2026-10-06）**

5. ✅ **`KnowNet` 最小宽度 500px**（`KnowList` 最小 300、`KnowDetail` 默认 500）。
6. ✅ **悬浮态 `KnowDetail` 允许拖宽**（拖左边缘），宽度持久化。
7. ✅ **钉住 / 取消钉住**：`KnowDetail` 面板**右上角的钉住图标按钮**（svg 图标），状态存 `setting` 表。
   - ⬜ 未定：`KnowDetail` 的**最小宽度**（默认 500 已定）。

### C. 阻塞 KnowNet 细节

8. **节点尺寸参数**：固定高度、字号、内边距、圆角、宽度自适应上限。
9. **缩放上下限**、锚点（鼠标位置 or 视口中心）是否定死。
10. **双击改标题的输入方式**：Konva 内嵌输入还是覆盖 DOM input；Enter / Esc / 失焦行为。
11. **拖拽连线与拖拽移动的分界**：节点边缘热区宽度、拖拽中的视觉提示、
    重复连线（已存在）与自连（拖到自己）如何处理。
12. **`node.move` 的写入时机**：拖拽结束写一次，还是过程中节流写入。
13. **空白处右键新建节点的坐标**（视口中心 or 右键位置）。
14. **是否支持多选 / 框选**（当前设计只有单选）。

### D. 阻塞 KnowDetail / KnowList 细节

15. **标题两处编辑的同步**：画布双击改标题与 `KnowDetail` 标题框改的是同一份数据，确认同步方式。
16. **防抖 flush 时机清单**（切换选中 / 关面板 / 切知识 / 关窗口）。
17. **详情中的图片**：roosterjs 插入图片存哪里（是否保留老 `ImageStore`）。
18. **KnowList**：新建对话框的取消方式；名称校验（空 / 重名 / 长度上限）；
    删除是否二次确认；删除后画布如何处理；列表排序规则。

### E. 文档自身的矛盾与过时（需回改，不影响开工）

- [32](./32-know-net.md)：「权重与布局的关系（**已选定 C**）」整节未标作废，与「权重已删除」冲突；
  「节点、连线与详细内容的关系」仍写「各自持有标题/权重」。
- ~~[40](./40-data-model.md)：`detail_id` 空/非空两处不一致~~ —— 已解决（A1），SQL 与说明均已改为非空。
- ~~[20](./20-data-layer.md)：类名 `KnowNode` 与 `NodesTable()` / `DbTableRegistrar<NodesTable>` 不一致~~
  —— 已改为 `KnowNode` / `DbTableRegistrar<KnowNode>`。
- [10](./10-overview.md)、[31](./31-content-box.md)：待确认项已被后续文档回答（权重、图结构、画布细节），需清理。
- [21](./21-ipc.md)：C++ 主动推送的**事件名清单**未列（`Msg.on` 目前没有对应事件）；
  是否需要一个「一次取回某知识全部数据」的聚合 method。
