# 40 · 数据模型（表结构）

数据层约定见 [20-data-layer.md](./20-data-layer.md)：`Db` 单例管连接，**一表一类**，表类是单例 + 实例方法。

## 一、用户初稿

| 表名 | 含义 | 列 |
| --- | --- | --- |
| `KnowNode` | 知识节点表 | `id`、知识标题 |
| `KnowLine` | 知识连线表 | `id`、权重 |
| `KnowDetail` | 知识详情表 | `id`、知识详情、**关联类型**（关联到 KnowNode 还是 KnowLine）、**关联ID** |
| `KnowList` | 知识列表表 | `id`、知识名称 |

四个表都再加 **创建时间、修改时间** 两列。

> 命名的语义提醒：`KnowList` 表存的是**一个个"知识"**（历史知识、编程知识、文学知识），
> 即列表里的每一项，与 `KnowList` 面板同名；`KnowNode` 存的是知识内部的**节点**。二者是「知识 → 节点」的两层。

## 二、评估与修改建议（待用户确认）

### 必须补充的列

| 表 | 缺失 | 说明 |
| --- | --- | --- |
| `KnowNode` | **所属知识（list_id）** | 否则无法做到「点击某个知识 → 加载这个知识的节点」 |
| `KnowLine` | **两个端点（node_a_id / node_b_id）** | 否则连线不知道连的是哪两个节点 |

### 建议调整：详情的关联方向反过来

初稿是 `KnowDetail` 持有「关联类型 + 关联ID」（多态反查）。缺点：**无法建外键约束**，
SQLite 的外键与级联删除全部失效，只能靠应用层保证完整性，删节点时详情容易变成孤儿数据。

**建议**：保留 `KnowDetail` 独立成表（把富文本这类大字段分离出去，避免查节点列表时带上大文本），
但**由 `KnowNode` / `KnowLine` 各自持有 `detail_id` 外键指向 `KnowDetail`**：

```
KnowNode.detail_id → KnowDetail.id
KnowLine.detail_id → KnowDetail.id
```

这样外键、级联都可用，`KnowDetail` 表不再需要「关联类型 / 关联ID」两列。
（代价：删除节点时需应用层或用触发器连带删除其 `KnowDetail` 行，因为外键的 CASCADE 方向是反的。）

### 已确认采纳的补充

- `KnowNode.list_id`（所属知识）、`KnowLine.node_a_id` / `node_b_id`（两个端点）；
- `KnowDetail` 的关联方向反过来，由 `KnowNode` / `KnowLine` 持有 `detail_id`；
- **`is_initial` 字段已删除**：数据库不区分"初始知识点"，它只是交互上的说法
  （空白右键新建的节点、或新建知识时的第一个节点）。

### 已确认：不允许跨知识连线

- **一条连线的两个端点必须属于同一个知识**（同一 `list_id`），不支持跨知识关联。
- 实现方式：`KnowLine` 的两端都指向 `KnowNode`，数据库层无法跨表 `CHECK`，
  因此**在表类（`KnowLine` 类）的写入方法里校验**两端 `list_id` 一致，不一致则拒绝写入。
- 影响：一个知识就是一张**独立的网**，知识之间互不影响；加载某知识时只需按 `list_id` 取节点与连线。

### 变更：改为手动摆放，坐标持久化 + 删除权重

> 用户决定**放弃力导向布局**，改为**用户手动拖拽摆放节点**。由此：

- **`KnowNode` 增加 `x / y`**：节点坐标必须持久化，否则用户摆的位置一重启就没了。
- **不需要 `pinned`**：没有自动布局，也就不存在"该节点是否参与计算"的问题。
- **`KnowLine` 删除 `weight` 列**：权重原本只用于驱动布局/距离，自动布局取消后失去意义。
  （详见 [32-know-net.md](./32-know-net.md) 的作废记录。）
- 连线的 `KnowDetail` 因此**只剩富文本详情**，不再有权重编辑框。

## 三、建表 SQL 草案（待用户确认后落地）

```sql
-- 知识（列表中的一项，一张网的入口）
CREATE TABLE IF NOT EXISTS know_list (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    name       TEXT    NOT NULL,
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL
);

-- 知识详情（富文本内容，与节点/连线分离存放）
CREATE TABLE IF NOT EXISTS know_detail (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    content    TEXT    NOT NULL DEFAULT '',
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL
);

-- 知识节点（坐标由用户拖拽摆放，持久化）
CREATE TABLE IF NOT EXISTS know_node (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    list_id    INTEGER NOT NULL REFERENCES know_list(id)   ON DELETE CASCADE,
    title      TEXT    NOT NULL DEFAULT '未命名',
    detail_id  INTEGER NOT NULL REFERENCES know_detail(id),  -- 非空，默认 RESTRICT
    x          REAL    NOT NULL DEFAULT 0,   -- 画布坐标（用户拖拽摆放）
    y          REAL    NOT NULL DEFAULT 0,
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL
);
CREATE INDEX IF NOT EXISTS idx_node_list ON know_node(list_id);

-- 知识连线（无向：约定 node_a_id < node_b_id）
CREATE TABLE IF NOT EXISTS know_line (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    node_a_id  INTEGER NOT NULL REFERENCES know_node(id)   ON DELETE CASCADE,
    node_b_id  INTEGER NOT NULL REFERENCES know_node(id)   ON DELETE CASCADE,
    detail_id  INTEGER NOT NULL REFERENCES know_detail(id),  -- 非空，默认 RESTRICT
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL,
    CHECK (node_a_id < node_b_id),           -- 无向边的唯一表示
    UNIQUE (node_a_id, node_b_id)            -- 两节点之间最多一条连线
);
CREATE INDEX IF NOT EXISTS idx_line_a ON know_line(node_a_id);
CREATE INDEX IF NOT EXISTS idx_line_b ON know_line(node_b_id);

-- 设置项（键值表，存放 UI 状态等持久化信息）
CREATE TABLE IF NOT EXISTS setting (
    key        TEXT PRIMARY KEY,
    value      TEXT NOT NULL,
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL
);
```

### `setting` 表（已确定新增）

用途：**存放需要持久化的设置/状态信息**。已知要存的项：

| key（草案） | value | 说明 |
| --- | --- | --- |
| `know_detail.pinned` | `0` / `1` | `KnowDetail` 面板是否处于钉住态（三列布局） |
| `know_list.width` | 像素值 | `KnowList` 面板宽度（splitter 拖动后的结果） |
| `know_detail.width` | 像素值 | `KnowDetail` 面板宽度 |

后续新增设置项直接加 key 即可，不必改表结构。

### 约定说明

- **主键**：`INTEGER PRIMARY KEY AUTOINCREMENT`（rowid 别名），简单够用。
- **时间**：`INTEGER` 存 Unix 毫秒时间戳，便于比较与排序（不用文本）。
- **无向边**：`CHECK (node_a_id < node_b_id)` + `UNIQUE` 联合保证「A-B 与 B-A 是同一条」且唯一。
- **坐标**：`x / y` 为画布坐标，由用户拖拽决定并持久化；**允许负坐标**，画布不限边界。
- **`detail_id` 非空（已确定，冲突已解决）**：节点/连线创建时**立即建一条空详情**并指向它，
  因此 `detail_id` 是 `NOT NULL`，前端不需要处理"没有详情"的分支。
  外键为默认 `RESTRICT`——删除时须**先删节点/连线，再删详情**（顺序反了会被拒）。
- **级联**：删除一个知识 → 连带删其节点 → 连带删相关连线（依赖 `PRAGMA foreign_keys = ON`，`Db.cpp` 已开启并校验）。
- **详情清理（已确定：应用层删除，不引触发器）**：删节点/连线/知识时在**表类的 `remove()` 里**
  先取回要清的 `detail_id`，删掉主体行之后再逐个删 `know_detail`（外键 CASCADE 方向是反的，数据库不会代劳）。
  - 删节点：先查出**它的所有连线的 `detail_id`**（连线会被外键级联删掉，详情要提前记下来），删节点后一并删详情。
  - 删知识：先查出该知识下所有节点与连线的 `detail_id`，删知识后一并删详情。
- **删除节点（已确定）**：连带删除**它的所有连线**，并删除**对应的 `KnowDetail` 记录**。
  连线本身由外键 `ON DELETE CASCADE` 自动清掉；详情需在表类里显式删除。

## 命名（已确定）

- **表名 / 列名一律小写下划线**：`know_list`、`know_node`、`know_line`、`know_detail`；
  字段如 `list_id`、`node_a_id`、`detail_id`、`created_at`、`updated_at`（`is_initial` 已删除，勿再使用）。
- **C++ 侧表类名不加 `Table` 后缀**，直接用：`KnowList` / `KnowNode` / `KnowLine` / `KnowDetail`
  （外加 `Setting`）。与前端面板同名无妨——二者分属 C++ 与 TS 两套代码，不会冲突。

## 待确认问题

- （暂无）`detail_id` 非空 ⇒ 详情与节点/连线**严格 1:1**，此前的疑问已消除。
