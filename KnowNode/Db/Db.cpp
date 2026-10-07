#include "Db.h"
#include "DbStmt.h"
#include "../Env.h"
#include "../Util.h"

Db& Db::instance()
{
    static Db db;
    return db;
}

Db::~Db()
{
    close();
}

void Db::init()
{
    open();
    createTables();
    // 外键放最后开：让建表这些一次性动作跑在跟升级前一样的环境里，
    // 万一旧库还留着不合外键的历史数据也不会卡住。此后的正常读写都在约束之下
    enableForeignKeys();
}

void Db::close()
{
    if (conn_)
    {
        sqlite3_close(conn_);
        conn_ = nullptr;
    }
}

void Db::registerTable(std::string name, std::function<void(sqlite3*)> create)
{
    tables_.push_back(Table{ std::move(name), std::move(create) });
}

bool Db::exec(const char* sql)
{
    return sqlite3_exec(conn_, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
}

void Db::execOrFatal(sqlite3* conn, const char* sql, const std::wstring& what)
{
    DbStmt stmt{ conn, sql };
    if (!stmt.ok() || !stmt.exec())
    {
        // 静态函数里拿不到 instance()，直接问连接要原因
        auto reason = sqlite3_errmsg(conn);
        auto detail = what + L"\n\n" + Util::convertToWStr(reason ? reason : "")
            + L"\n\nSQL：" + Util::convertToWStr(sql);
        fatal(detail);
    }
}

void Db::ensureColumn(sqlite3* conn, const char* table, const char* column, const char* definition)
{
    // PRAGMA table_info 不接受参数绑定，表名是代码里的常量，直接拼进 SQL
    std::string info = "PRAGMA table_info(";
    info += table;
    info += ");";
    DbStmt stmt{ conn, info.c_str() };
    while (stmt.step())
    {
        // table_info 的第 2 列是列名
        if (stmt.columnText(1) == column) return;
    }

    std::string alter = "ALTER TABLE ";
    alter += table;
    alter += " ADD COLUMN ";
    alter += column;
    alter += " ";
    alter += definition;
    alter += ";";
    auto what = std::wstring{ L"给表补字段失败\n\n表：" } + Util::convertToWStr(table)
        + L"\n字段：" + Util::convertToWStr(column);
    execOrFatal(conn, alter.c_str(), what);
}

std::string Db::lastError() const
{
    auto msg = conn_ ? sqlite3_errmsg(conn_) : "no connection";
    return msg ? std::string(msg) : std::string();
}

void Db::fatal(const std::wstring& detail)
{
    auto msg = std::wstring{ L"数据库初始化失败\n\n" } + detail;
    MessageBox(nullptr, msg.c_str(), L"系统提示", MB_OK | MB_ICONERROR);
    ExitProcess(-1);
}

void Db::open()
{
    // 数据目录由 Env::initDataPath 负责创建；db.db 不存在时 sqlite3 会自动创建文件
    auto dbPath = Env::getDataPath() / L"db.db";
    // sqlite3 按 UTF-8 解析路径，这里显式转 UTF-8：既避免 path::string() 的 ANSI 转换
    // 在中文用户名下打不开库，也不用 sqlite3_open16 —— 后者会顺手执行
    // PRAGMA encoding='UTF-16'，把新库的默认编码从 UTF-8 改成 UTF-16le
    auto u8Path = dbPath.u8string();
    auto rc = sqlite3_open(reinterpret_cast<const char*>(u8Path.c_str()), &conn_);
    if (rc != SQLITE_OK)
    {
        auto detail = L"无法打开数据库文件\n\n" + dbPath.wstring() + L"\n\n"
            + (conn_ ? std::wstring{ static_cast<const wchar_t*>(sqlite3_errmsg16(conn_)) }
                : std::wstring{ L"无法创建数据库连接" });
        sqlite3_close(conn_);
        conn_ = nullptr;
        fatal(detail);
    }
}

void Db::createTables()
{
    for (auto& table : tables_)
    {
        table.create(conn_);
    }
}

void Db::enableForeignKeys()
{
    if (sqlite3_exec(conn_, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr) != SQLITE_OK)
        fatal(L"启用外键约束失败\n\n" + std::wstring{ static_cast<const wchar_t*>(sqlite3_errmsg16(conn_)) });

    // PRAGMA 执行成功不等于真的开了：编译期去掉外键支持时它是静默无效的，读回来确认一下
    DbStmt stmt{ conn_, "PRAGMA foreign_keys;" };
    bool on = false;
    if (stmt.step()) on = stmt.columnInt(0) != 0;
    if (!on) fatal(L"SQLite 未启用外键支持\n\n这一版 sqlite3 可能是在关闭外键的情况下编译的");
}
