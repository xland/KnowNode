#include "Setting.h"
#include "Db.h"
#include "DbStmt.h"
#include "../Util.h"

static const DbTableRegistrar<Setting> registrar;

Setting::Setting()
{
    Db::instance().registerTable("setting", [](sqlite3* conn) {
        Setting::instance().create(conn);
    });
}

Setting& Setting::instance()
{
    static Setting table;
    return table;
}

void Setting::create(sqlite3* conn)
{
    const char* sql =
        "CREATE TABLE IF NOT EXISTS setting ("
        "    key        TEXT PRIMARY KEY,"
        "    value      TEXT NOT NULL,"
        "    created_at INTEGER NOT NULL,"
        "    updated_at INTEGER NOT NULL"
        ");";
    Db::execOrFatal(conn, sql, L"创建 setting 表失败");
}

std::string Setting::get(const std::string& key, const std::string& fallback) const
{
    DbStmt stmt{ Db::instance().conn(), "SELECT value FROM setting WHERE key = ?;" };
    if (!stmt.ok()) return fallback;
    stmt.bindText(1, key);
    return stmt.step() ? stmt.columnText(0) : fallback;
}

void Setting::set(const std::string& key, const std::string& value)
{
    auto now = Util::nowMillis();
    sqlite3* conn = Db::instance().conn();

    // 先试着更新；没更新到任何一行说明还不存在，再插入（比依赖 UPSERT 的 sqlite 版本更稳妥）
    DbStmt update{ conn, "UPDATE setting SET value = ?, updated_at = ? WHERE key = ?;" };
    if (update.ok())
    {
        update.bindText(1, value);
        update.bindInt(2, now);
        update.bindText(3, key);
        update.run();
        if (sqlite3_changes(conn) > 0) return;
    }

    DbStmt insert{ conn,
        "INSERT INTO setting (key, value, created_at, updated_at) VALUES (?, ?, ?, ?);" };
    if (!insert.ok()) return;
    insert.bindText(1, key);
    insert.bindText(2, value);
    insert.bindInt(3, now);
    insert.bindInt(4, now);
    insert.run();
}

int64_t Setting::getInt(const std::string& key, int64_t fallback) const
{
    auto text = get(key);
    if (text.empty()) return fallback;
    try
    {
        return std::stoll(text);
    }
    catch (const std::exception&)
    {
        return fallback;
    }
}
