#include "KnowDetail.h"
#include "Db.h"
#include "DbStmt.h"
#include "../Util.h"

static const DbTableRegistrar<KnowDetail> registrar;

KnowDetail::KnowDetail()
{
    Db::instance().registerTable("know_detail", [](sqlite3* conn) {
        KnowDetail::instance().create(conn);
    });
}

KnowDetail& KnowDetail::instance()
{
    static KnowDetail table;
    return table;
}

void KnowDetail::create(sqlite3* conn)
{
    const char* sql =
        "CREATE TABLE IF NOT EXISTS know_detail ("
        "    id         INTEGER PRIMARY KEY AUTOINCREMENT,"
        "    content    TEXT    NOT NULL DEFAULT '',"
        "    created_at INTEGER NOT NULL,"
        "    updated_at INTEGER NOT NULL"
        ");";
    Db::execOrFatal(conn, sql, L"创建 know_detail 表失败");
}

int64_t KnowDetail::add(const std::string& content)
{
    auto now = Util::nowMillis();
    sqlite3* conn = Db::instance().conn();
    DbStmt stmt{ conn, "INSERT INTO know_detail (content, created_at, updated_at) VALUES (?, ?, ?);" };
    if (!stmt.ok()) return 0;
    stmt.bindText(1, content);
    stmt.bindInt(2, now);
    stmt.bindInt(3, now);
    stmt.run();
    return sqlite3_last_insert_rowid(conn);
}

bool KnowDetail::save(int64_t id, const std::string& content)
{
    DbStmt stmt{ Db::instance().conn(),
        "UPDATE know_detail SET content = ?, updated_at = ? WHERE id = ?;" };
    if (!stmt.ok()) return false;
    stmt.bindText(1, content);
    stmt.bindInt(2, Util::nowMillis());
    stmt.bindInt(3, id);
    stmt.run();
    return sqlite3_changes(Db::instance().conn()) > 0;
}

std::string KnowDetail::content(int64_t id) const
{
    DbStmt stmt{ Db::instance().conn(), "SELECT content FROM know_detail WHERE id = ?;" };
    if (!stmt.ok()) return std::string();
    stmt.bindInt(1, id);
    return stmt.step() ? stmt.columnText(0) : std::string();
}

bool KnowDetail::remove(int64_t id)
{
    DbStmt stmt{ Db::instance().conn(), "DELETE FROM know_detail WHERE id = ?;" };
    if (!stmt.ok()) return false;
    stmt.bindInt(1, id);
    stmt.run();
    return sqlite3_changes(Db::instance().conn()) > 0;
}
