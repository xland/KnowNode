#include "KnowList.h"
#include "Db.h"
#include "DbStmt.h"
#include "KnowDetail.h"
#include "../Util.h"

static const DbTableRegistrar<KnowList> registrar;

KnowList::KnowList()
{
    Db::instance().registerTable("know_list", [](sqlite3* conn) {
        KnowList::instance().create(conn);
    });
}

KnowList& KnowList::instance()
{
    static KnowList table;
    return table;
}

void KnowList::create(sqlite3* conn)
{
    const char* sql =
        "CREATE TABLE IF NOT EXISTS know_list ("
        "    id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    name       TEXT    NOT NULL,"
        "    created_at INTEGER NOT NULL,"
        "    updated_at INTEGER NOT NULL"
        ");";
    if (!DbStmt{ conn, sql }.ok())
        Db::fatal(L"创建 know_list 表失败");
}

int64_t KnowList::add(const std::string& name)
{
    auto now = Util::nowMillis();
    sqlite3* conn = Db::instance().conn();
    DbStmt stmt{ conn, "INSERT INTO know_list (name, created_at, updated_at) VALUES (?, ?, ?);" };
    if (!stmt.ok()) return 0;
    stmt.bindText(1, name);
    stmt.bindInt(2, now);
    stmt.bindInt(3, now);
    stmt.run();
    return sqlite3_last_insert_rowid(conn);
}

bool KnowList::rename(int64_t id, const std::string& name)
{
    DbStmt stmt{ Db::instance().conn(),
        "UPDATE know_list SET name = ?, updated_at = ? WHERE id = ?;" };
    if (!stmt.ok()) return false;
    stmt.bindText(1, name);
    stmt.bindInt(2, Util::nowMillis());
    stmt.bindInt(3, id);
    stmt.run();
    return sqlite3_changes(Db::instance().conn()) > 0;
}

bool KnowList::remove(int64_t id)
{
    sqlite3* conn = Db::instance().conn();

    // 删知识会级联删掉它的节点、连线，但那些节点/连线各自的 know_detail 行不会自动跟着走
    // （外键是它们指向 detail，不是反过来），所以先记下要清理的 detail id
    std::vector<int64_t> detailIds;
    DbStmt nodes{ conn, "SELECT detail_id FROM know_node WHERE list_id = ?;" };
    nodes.bindInt(1, id);
    while (nodes.step()) detailIds.push_back(nodes.columnInt(0));

    DbStmt lines{ conn,
        "SELECT l.detail_id FROM know_line l"
        "  JOIN know_node n ON n.id = l.node_a_id"
        " WHERE n.list_id = ?;" };
    lines.bindInt(1, id);
    while (lines.step()) detailIds.push_back(lines.columnInt(0));

    DbStmt del{ conn, "DELETE FROM know_list WHERE id = ?;" };
    if (!del.ok()) return false;
    del.bindInt(1, id);
    del.run();
    if (sqlite3_changes(conn) == 0) return false;

    for (auto detailId : detailIds) KnowDetail::instance().remove(detailId);
    return true;
}

std::vector<KnowListItem> KnowList::all() const
{
    std::vector<KnowListItem> items;
    DbStmt stmt{ Db::instance().conn(),
        "SELECT id, name, created_at, updated_at FROM know_list ORDER BY created_at, id;" };
    if (!stmt.ok()) return items;
    while (stmt.step())
    {
        KnowListItem item;
        item.id = stmt.columnInt(0);
        item.name = stmt.columnText(1);
        item.createdAt = stmt.columnInt(2);
        item.updatedAt = stmt.columnInt(3);
        items.push_back(std::move(item));
    }
    return items;
}

bool KnowList::get(int64_t id, KnowListItem& out) const
{
    DbStmt stmt{ Db::instance().conn(),
        "SELECT id, name, created_at, updated_at FROM know_list WHERE id = ?;" };
    if (!stmt.ok()) return false;
    stmt.bindInt(1, id);
    if (!stmt.step()) return false;
    out.id = stmt.columnInt(0);
    out.name = stmt.columnText(1);
    out.createdAt = stmt.columnInt(2);
    out.updatedAt = stmt.columnInt(3);
    return true;
}
