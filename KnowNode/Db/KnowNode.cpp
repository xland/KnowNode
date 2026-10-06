#include "KnowNode.h"
#include "Db.h"
#include "DbStmt.h"
#include "KnowDetail.h"
#include "../Util.h"

static const DbTableRegistrar<KnowNode> registrar;

KnowNode::KnowNode()
{
    Db::instance().registerTable("know_node", [](sqlite3* conn) {
        KnowNode::instance().create(conn);
    });
}

KnowNode& KnowNode::instance()
{
    static KnowNode table;
    return table;
}

void KnowNode::create(sqlite3* conn)
{
    const char* sql =
        "CREATE TABLE IF NOT EXISTS know_node ("
        "    id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    list_id    INTEGER NOT NULL REFERENCES know_list(id)   ON DELETE CASCADE,"
        "    title      TEXT    NOT NULL DEFAULT '未命名',"
        "    detail_id  INTEGER NOT NULL REFERENCES know_detail(id),"
        "    x          REAL    NOT NULL DEFAULT 0,"
        "    y          REAL    NOT NULL DEFAULT 0,"
        "    created_at INTEGER NOT NULL,"
        "    updated_at INTEGER NOT NULL"
        ");";
    if (!DbStmt{ conn, sql }.ok())
        Db::fatal(L"创建 know_node 表失败");

    const char* index = "CREATE INDEX IF NOT EXISTS idx_node_list ON know_node(list_id);";
    if (!DbStmt{ conn, index }.ok())
        Db::fatal(L"创建 know_node 索引失败");
}

int64_t KnowNode::add(int64_t listId, double x, double y, const std::string& title)
{
    // 节点一定有一条详情，先建详情拿到 id，节点那条记录再指向它
    auto detailId = KnowDetail::instance().add();
    if (detailId == 0) return 0;

    auto now = Util::nowMillis();
    sqlite3* conn = Db::instance().conn();
    DbStmt stmt{ conn,
        "INSERT INTO know_node (list_id, title, detail_id, x, y, created_at, updated_at)"
        " VALUES (?, ?, ?, ?, ?, ?, ?);" };
    if (!stmt.ok()) return 0;
    stmt.bindInt(1, listId);
    stmt.bindText(2, title);
    stmt.bindInt(3, detailId);
    stmt.bindDouble(4, x);
    stmt.bindDouble(5, y);
    stmt.bindInt(6, now);
    stmt.bindInt(7, now);
    stmt.run();
    if (sqlite3_changes(conn) == 0) return 0;
    return sqlite3_last_insert_rowid(conn);
}

bool KnowNode::updateTitle(int64_t id, const std::string& title)
{
    DbStmt stmt{ Db::instance().conn(),
        "UPDATE know_node SET title = ?, updated_at = ? WHERE id = ?;" };
    if (!stmt.ok()) return false;
    stmt.bindText(1, title);
    stmt.bindInt(2, Util::nowMillis());
    stmt.bindInt(3, id);
    stmt.run();
    return sqlite3_changes(Db::instance().conn()) > 0;
}

bool KnowNode::move(int64_t id, double x, double y)
{
    DbStmt stmt{ Db::instance().conn(),
        "UPDATE know_node SET x = ?, y = ?, updated_at = ? WHERE id = ?;" };
    if (!stmt.ok()) return false;
    stmt.bindDouble(1, x);
    stmt.bindDouble(2, y);
    stmt.bindInt(3, Util::nowMillis());
    stmt.bindInt(4, id);
    stmt.run();
    return sqlite3_changes(Db::instance().conn()) > 0;
}

bool KnowNode::remove(int64_t id)
{
    sqlite3* conn = Db::instance().conn();

    KnowNodeItem item;
    if (!get(id, item)) return false;

    // 连到这个节点的连线会被外键级联删掉，但那些连线各自的详情要在这儿记下来手动清
    std::vector<int64_t> lineDetailIds;
    DbStmt lines{ conn, "SELECT detail_id FROM know_line WHERE node_a_id = ? OR node_b_id = ?;" };
    lines.bindInt(1, id);
    lines.bindInt(2, id);
    while (lines.step()) lineDetailIds.push_back(lines.columnInt(0));

    DbStmt del{ conn, "DELETE FROM know_node WHERE id = ?;" };
    if (!del.ok()) return false;
    del.bindInt(1, id);
    del.run();
    if (sqlite3_changes(conn) == 0) return false;

    for (auto detailId : lineDetailIds) KnowDetail::instance().remove(detailId);
    KnowDetail::instance().remove(item.detailId);
    return true;
}

std::vector<KnowNodeItem> KnowNode::ofList(int64_t listId) const
{
    std::vector<KnowNodeItem> items;
    DbStmt stmt{ Db::instance().conn(),
        "SELECT id, list_id, title, detail_id, x, y, created_at, updated_at"
        "  FROM know_node WHERE list_id = ? ORDER BY created_at, id;" };
    if (!stmt.ok()) return items;
    stmt.bindInt(1, listId);
    while (stmt.step())
    {
        KnowNodeItem item;
        item.id = stmt.columnInt(0);
        item.listId = stmt.columnInt(1);
        item.title = stmt.columnText(2);
        item.detailId = stmt.columnInt(3);
        item.x = stmt.columnDouble(4);
        item.y = stmt.columnDouble(5);
        item.createdAt = stmt.columnInt(6);
        item.updatedAt = stmt.columnInt(7);
        items.push_back(std::move(item));
    }
    return items;
}

bool KnowNode::get(int64_t id, KnowNodeItem& out) const
{
    DbStmt stmt{ Db::instance().conn(),
        "SELECT id, list_id, title, detail_id, x, y, created_at, updated_at"
        "  FROM know_node WHERE id = ?;" };
    if (!stmt.ok()) return false;
    stmt.bindInt(1, id);
    if (!stmt.step()) return false;
    out.id = stmt.columnInt(0);
    out.listId = stmt.columnInt(1);
    out.title = stmt.columnText(2);
    out.detailId = stmt.columnInt(3);
    out.x = stmt.columnDouble(4);
    out.y = stmt.columnDouble(5);
    out.createdAt = stmt.columnInt(6);
    out.updatedAt = stmt.columnInt(7);
    return true;
}

int64_t KnowNode::listIdOf(int64_t id) const
{
    DbStmt stmt{ Db::instance().conn(), "SELECT list_id FROM know_node WHERE id = ?;" };
    if (!stmt.ok()) return 0;
    stmt.bindInt(1, id);
    return stmt.step() ? stmt.columnInt(0) : 0;
}
