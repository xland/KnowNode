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
4. **建表语句必须走 `Db::execOrFatal(conn, sql, 失败提示)`，不要自己拿 `DbStmt` 只判 `ok()`**
   （2026-10-06 实踩）：`DbStmt` 构造时只 `prepare`，`ok()` 只表示 SQL 能编译，**不代表执行过**。
   只 prepare 不 step 的 `CREATE TABLE` 等于没建，紧接着引用该表的 `CREATE INDEX` 会在 prepare 阶段
   报 `no such table`——表象是"创建 xxx 索引失败"，真因是表压根没建。
5. **不做老库兼容（2026-10-08 定）**：项目尚未发布、没有既有用户数据，
   **不支持已有的 `db.db`**。所有列一律写在 `CREATE TABLE` 里，`Db::ensureColumn()` 与
   `KnowList::ensureDetail()` 已删除。真遇到旧 `db.db` 就删掉重建，不写任何升级/迁移代码。

## 类结构（**已按此落地**，与 `KnowNode/Db/Db.h` 现状一致）

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
    void registerTable(std::string name, std::function<void(sqlite3*)> create);

    // 建表 / 建索引：prepare + step，任一步失败就带 SQLite 的原因 fatal
    static void execOrFatal(sqlite3* conn, const char* sql, const std::wstring& what);

private:
    Db() = default;                    // 单例：禁止外部构造
    sqlite3* conn_ = nullptr;
    std::vector<Table> tables_;        // 注册表，按注册顺序建表
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
    KnowNode();                        // 构造时向 Db::instance().registerTable(...) 注册本表
};
```

自注册做法（避免 `Db` 里出现一长串表清单）：在每个表类 `.cpp` 中放一个文件级静态注册器，
它在静态初始化期构造表单例并完成注册；`Db::init()` 只需遍历已注册表。
`Db::instance()` 用**函数内静态对象**，因此表类在静态初始化期引用它是安全的（首次调用才构造）。

```cpp
// Db/KnowNode.cpp
static DbTableRegistrar<KnowNode> g_knowNode;   // main() 之前完成注册
```

> **已确认并落地**：自注册方案认可，代码已在 `KnowNode/Db/` 实现
> （`Db.h` / `Db.cpp` + `KnowList` / `KnowNode` / `KnowLine` / `KnowDetail` / `Setting` 五个表类）。

## 现状代码（**已按新约定落地**，2026-10-08 核对）

> **本节原写的是老状态，已整段作废**：原文称 `Db.h` 里是 `namespace Db` 的两个自由函数
> （`init()` / `get()`）、`Db.cpp` 里 `createSchema()` 为空——**这两条都不成立**。
> 现在 `Db` 就是上面草案里那个**单例类**，建表由五个表类自注册，`createSchema()` 这个函数**根本不存在**。

- `Db` **是类、是单例**（`class Db { static Db& instance(); ... }`，拷贝构造与赋值已 `delete`），
  **不是 `namespace Db`**。入口是 `Db::instance()`，取连接是 `Db::instance().conn()`。
- 建表动作由五个表类（`KnowList` / `KnowNode` / `KnowLine` / `KnowDetail` / `Setting`）
  在构造时注册，`Db::init()` 只遍历注册表；`Db` 里**没有任何硬编码的建表 SQL**。
- 建表语句走 `Db::execOrFatal()`；**不支持老库升级**，所以没有、也不再需要 `ensureColumn()`
  （见「已确定的设计决策」5）。
- 已有实现细节（保留）：连接用 UTF-8 路径打开（避免中文用户名问题）、`PRAGMA foreign_keys = ON` 且读回校验。
- 老项目的 `Category` / `Article` 表及"写入测试数据"逻辑**已不存在**；`migrateImageTable()` 也不存在
  （没有 `image` 表，见下节）。

## 旧代码清理（**已确定**）

**删除**（老项目「文章」相关，与新架构无关）：

- 前端组件：`UI/src/ArticleEditor/`、`UI/src/ArticleTitle/`、`UI/src/EditorTitle/`；`ContentBox.ts` 里对它们的引用。
- 表与迁移：`category` / `article` 表的建表与迁移逻辑。
- 通信 method：`getImageDir` 之外的老 method 一律按 [21](./21-ipc.md) 的新命名重写。

**保留**（`packages/` 不动；图片链条见 [33-know-detail.md](./33-know-detail.md)）：

- `UI/src/ImageStore.ts`、`UI/src/EditorContent/ImagePlugin.ts`、`UI/src/EditorBar/Image/`。
- C++ 侧 `Page::handleGetImageDir`（method 名 `image.dir`）与数据目录下的 `images` 子目录。

> **「老 `image` 表」这条是错的，已删（2026-10-08 核对）**：库里只有
> `know_list` / `know_node` / `know_line` / `know_detail` / `setting` **五张表**，
> 既没有 `image` 表，也没有 `migrateImageTable()`——图片只落在 `images` 目录里，
> 正文存文件名，**没有任何表记录"正文引用了哪些图片"**，前端 `ImageStore.ts` 也只调 `image.dir`。
> 同段落里的 `Page::handleResizeImage`、`ImageResize.ts` 也已删除（图片不再另存缩放图）。

## 待确认问题

- （暂无）类结构草案、自注册方式、表结构均已确认并落地。
