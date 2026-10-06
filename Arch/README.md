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
| [34-know-list.md](./34-know-list.md) | KnowList 面板：新建/重命名/删除知识 | 已记录 |
| [30-frontend.md](./30-frontend.md) | 前端：`Msg.ts` 通信、三大布局组件、`WindowBorder` | 已记录 |
| [31-content-box.md](./31-content-box.md) | 容器区：KnowList / KnowNet / KnowDetail，splitter 规则 | 已记录 |
| [32-know-net.md](./32-know-net.md) | KnowNet 画布：节点、贝塞尔连线、力导向布局、缩放 | 已记录 |
| [33-know-detail.md](./33-know-detail.md) | KnowDetail：节点（标题+详情）/ 连线（权重+详情） | 已记录 |
| [40-data-model.md](./40-data-model.md) | 数据模型：know_list / know_node / know_line / know_detail / setting | 待确认 |

## 待确认问题

- 数据层「类结构草案」是否认可（自注册方式）。
- 首个表的定义（字段）。
- `ContentBox` 内部分区；`Article*`/`Editor*` 旧组件去留。
