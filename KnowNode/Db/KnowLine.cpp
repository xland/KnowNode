#include "KnowLine.h"
#include "Db.h"
#include "DbStmt.h"
#include "KnowDetail.h"
#include "KnowNode.h"
#include "../Util.h"

static const DbTableRegistrar<KnowLine> registrar;

KnowLine::KnowLine()
{
    Db::instance().registerTable("know_line", [](sqlite3* conn) {
        KnowLine::instance().create(conn);
    });
}

KnowLine& KnowLine::instance()
{
    static KnowLine table;
    return table;
}

void KnowLine::create(sqlite3* conn)
{
    const char* sql =
        "CREATE TABLE IF NOT EXISTS know_line ("
        "    id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    node_a_id  INTEGER NOT NULL REFERENCES know_node(id)   ON DELETE CASCADE,"
        "    node_b_id  INTEGER NOT NULL REFERENCES know_node(id)   ON DELETE CASCADE,"
        "    detail_id  INTEGER NOT NULL REFERENCES know_detail(id),"
        "    created_at INTEGER NOT NULL,"
        "    updated_at INTEGER NOT NULL,"
        "    color      INTEGER NOT NULL DEFAULT 0,"
        "    CHECK (node_a_id < node_b_id),"
        "    UNIQUE (node_a_id, node_b_id)"
        ");";
    Db::execOrFatal(conn, sql, L"创建 know_line 表失败");

    Db::execOrFatal(conn, "CREATE INDEX IF NOT EXISTS idx_line_a ON know_line(node_a_id);", L"创建 know_line 索引 idx_line_a 失败");
    Db::execOrFatal(conn, "CREATE INDEX IF NOT EXISTS idx_line_b ON know_line(node_b_id);", L"创建 know_line 索引 idx_line_b 失败");
}

int64_t KnowLine::add(int64_t nodeAId, int64_t nodeBId)
{
    if (nodeAId == nodeBId) return 0;

    // 连线不跨知识：两端必须属于同一个 know_list
    auto listA = KnowNode::instance().listIdOf(nodeAId);
    auto listB = KnowNode::instance().listIdOf(nodeBId);
    if (listA == 0 || listA != listB) return 0;

    // 无向：统一小的在前，这样 A-B 与 B-A 落到同一条记录上
    auto a = nodeAId < nodeBId ? nodeAId : nodeBId;
    auto b = nodeAId < nodeBId ? nodeBId : nodeAId;

    sqlite3* conn = Db::instance().conn();

    // 已经有这条关联就直接复用，不重复建
    DbStmt exist{ conn, "SELECT id FROM know_line WHERE node_a_id = ? AND node_b_id = ?;" };
    if (exist.ok())
    {
        exist.bindInt(1, a);
        exist.bindInt(2, b);
        if (exist.step()) return exist.columnInt(0);
    }

    auto detailId = KnowDetail::instance().add();
    if (detailId == 0) return 0;

    auto now = Util::nowMillis();
    DbStmt stmt{ conn,
        "INSERT INTO know_line (node_a_id, node_b_id, detail_id, created_at, updated_at)"
        " VALUES (?, ?, ?, ?, ?);" };
    if (!stmt.ok()) return 0;
    stmt.bindInt(1, a);
    stmt.bindInt(2, b);
    stmt.bindInt(3, detailId);
    stmt.bindInt(4, now);
    stmt.bindInt(5, now);
    stmt.run();
    if (sqlite3_changes(conn) == 0) return 0;
    return sqlite3_last_insert_rowid(conn);
}

bool KnowLine::remove(int64_t id)
{
    sqlite3* conn = Db::instance().conn();

    KnowLineItem item;
    if (!get(id, item)) return false;

    DbStmt del{ conn, "DELETE FROM know_line WHERE id = ?;" };
    if (!del.ok()) return false;
    del.bindInt(1, id);
    del.run();
    if (sqlite3_changes(conn) == 0) return false;

    KnowDetail::instance().remove(item.detailId);
    return true;
}

bool KnowLine::setColor(int64_t id, int color)
{
    if (color < 0) color = 0;
    if (color > maxColor) color = maxColor;

    DbStmt stmt{ Db::instance().conn(),
        "UPDATE know_line SET color = ?, updated_at = ? WHERE id = ?;" };
    if (!stmt.ok()) return false;
    stmt.bindInt(1, color);
    stmt.bindInt(2, Util::nowMillis());
    stmt.bindInt(3, id);
    stmt.run();
    return sqlite3_changes(Db::instance().conn()) > 0;
}

std::vector<KnowLineItem> KnowLine::ofList(int64_t listId) const
{
    std::vector<KnowLineItem> items;
    DbStmt stmt{ Db::instance().conn(),
        "SELECT l.id, l.node_a_id, l.node_b_id, l.detail_id, l.created_at, l.updated_at, l.color"
        "  FROM know_line l"
        "  JOIN know_node n ON n.id = l.node_a_id"
        " WHERE n.list_id = ?"
        " ORDER BY l.created_at, l.id;" };
    if (!stmt.ok()) return items;
    stmt.bindInt(1, listId);
    while (stmt.step())
    {
        KnowLineItem item;
        item.id = stmt.columnInt(0);
        item.nodeAId = stmt.columnInt(1);
        item.nodeBId = stmt.columnInt(2);
        item.detailId = stmt.columnInt(3);
        item.createdAt = stmt.columnInt(4);
        item.updatedAt = stmt.columnInt(5);
        item.color = static_cast<int>(stmt.columnInt(6));
        items.push_back(std::move(item));
    }
    return items;
}

bool KnowLine::get(int64_t id, KnowLineItem& out) const
{
    DbStmt stmt{ Db::instance().conn(),
        "SELECT id, node_a_id, node_b_id, detail_id, created_at, updated_at"
        "  FROM know_line WHERE id = ?;" };
    if (!stmt.ok()) return false;
    stmt.bindInt(1, id);
    if (!stmt.step()) return false;
    out.id = stmt.columnInt(0);
    out.nodeAId = stmt.columnInt(1);
    out.nodeBId = stmt.columnInt(2);
    out.detailId = stmt.columnInt(3);
    out.createdAt = stmt.columnInt(4);
    out.updatedAt = stmt.columnInt(5);
    out.color = static_cast<int>(stmt.columnInt(6));
    return true;
}
