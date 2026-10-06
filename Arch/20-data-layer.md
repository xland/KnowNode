# 20 · 数据层（SQLite / C++）

## 存储选型

使用 **SQLite** 保存用户录入的知识。

## 目录约定

| 路径 | 职责 |
| --- | --- |
| `KnowNode/SQLite/` | SQLite 源码（`sqlite3.c` / `sqlite3.h`），第三方库，不改动 |
| `KnowNode/Db/` | **所有数据库相关代码** |

## 分层约定

### 1. `Db` —— 数据库管理类

- 位置：`KnowNode/Db/Db.h`（+ `Db.cpp`）
- 职责：
  - **初始化数据库**（建库、建表）；
  - **管理数据库连接**。
- 它是整个应用唯一的数据库入口，其他模块不自行打开连接。

### 2. 每张表一个类

- 将来会创建很多表，**每个表对应一个类**，全部放在 `KnowNode/Db/` 下。
- 调用方**只通过该表类提供的方法访问数据**，不直接拼 SQL、不直接拿连接去查。

> 即：`Db` 管连接与 schema，各表类管自己那张表的读写；二者职责不交叉。

## 已确定的设计决策

1. **`Db` 是真正的类、单例**（不再是 `namespace Db` 自由函数）。
2. **建表由各表类自己注册**：`Db` 不硬编码任何建表 SQL，只负责在 `init()` 时执行已注册的建表动作。
3. **每个表类都是单例类，对外接口全部是实例方法**（不提供静态 CRUD）。

## 类结构草案（实现草案，待用户确认后落地）

```cpp
// Db/Db.h —— 数据库管理单例
class Db
{
public:
    static Db& instance();             // 单例入口，取代原 Db::init()/Db::get()

    void init();                       // 打开 db.db → 执行所有已注册表的建表 → 开启外键
    sqlite3* conn() const;             // 取连接，供表类执行 SQL
    void close();

    // 表类自注册入口：传表名 + 建表回调（回调内执行 CREATE TABLE IF NOT EXISTS ...）
    void registerTable(std::string_view name, std::function<void(sqlite3*)> create);

private:
    Db() = default;                    // 单例：禁止外部构造
    sqlite3* conn_ = nullptr;
    std::vector<Entry> tables_;        // 注册表，按注册顺序建表
};
```

```cpp
// Db/Nodes 等表类写法示例（类名不加 Table 后缀）
class KnowNode
{
public:
    static KnowNode& instance();       // 表类也是单例

    void create(sqlite3* conn);        // 建表：CREATE TABLE IF NOT EXISTS ...

    // 以下为实例方法，是对外唯一的访问途径
    // void insert(...);  std::optional<Node> findById(...);  ...

private:
    NodesTable();                      // 构造时向 Db::instance().registerTable(...) 注册本表
};
```

自注册做法（避免 `Db` 里出现一长串表清单）：在每个表类 `.cpp` 中放一个文件级静态注册器，
它在静态初始化期构造表单例并完成注册；`Db::init()` 只需遍历已注册表。

```cpp
// Db/NodesTable.cpp
static DbTableRegistrar<NodesTable> g_nodesTable;   // main() 之前完成注册
```

## 现状代码（含老项目痕迹，需按新约定改造）

`KnowNode/Db/Db.h` 目前是 `namespace Db` 的两个自由函数：

```cpp
namespace Db
{
    void init();      // 打开数据目录下的 db.db，建表
    sqlite3* get();   // 取连接，供表类执行 SQL
}
```

- `Db.cpp` 中 `createSchema()` 当前为**空**——按新约定，未来的建表动作应由各表类提供，由 `Db::init()` 统一调用。
- 已有实现细节（保留）：连接用 UTF-8 路径打开（避免中文用户名问题）、`PRAGMA foreign_keys = ON` 且读回校验。
- 老项目残留，待清理：
  - `migrateImageTable()`：旧库 `image` 表的 `img_path` 列改名对齐 `img_name`，注释自述"确认没人用旧库后可删除"。
  - 注释中提到的 `Category` / `Article` 表及"写入测试数据"逻辑，属于旧项目的表结构，不纳入新架构。

## 待确认问题

- 上述「类结构草案」是否认可（尤其是「文件级静态注册器 + `Db::init()` 遍历」的自注册方式）。
- 首个要建的表是什么（知识节点表？关联表？），以及它的字段。
- 已有 `migrateImageTable()` 是否随旧库一起废弃删除。
