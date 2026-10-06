#include "Db.h"
#include "../Env.h"
#include <cstring>

namespace
{
    /// 到 db.db 的持久连接：进程内只用这一个，Db::get() 直接把它交出去
    sqlite3* conn = nullptr;

    /// 数据库开不起来/建不了表就没法往下走，弹提示后直接结束进程
    [[noreturn]] void fatal(const std::wstring& detail)
    {
        auto msg = std::wstring{ L"数据库初始化失败\n\n" } + detail;
        MessageBox(nullptr, msg.c_str(), L"系统提示", MB_OK | MB_ICONERROR);
        ExitProcess(-1);
    }

    void createSchema()
    {
        
    }

    /// 旧库里的 image 表这一列还叫 img_path（存的还是 images/xxx.png 这种相对路径），
    /// 这里把它改名并顺手去掉路径前缀，跟新表的 img_name 对齐；没建过这张表则什么都不做。
    /// 等确认没人用旧库了，本函数和下面的调用点可以一起删掉
    void migrateImageTable()
    {
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(conn, "PRAGMA table_info(image);", -1, &stmt, nullptr) != SQLITE_OK)
            return;

        bool hasOldColumn = false;
        while (sqlite3_step(stmt) == SQLITE_ROW)
        {
            auto name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            if (name && std::strcmp(name, "img_path") == 0) hasOldColumn = true;
        }
        sqlite3_finalize(stmt);
        if (!hasOldColumn) return;

        // sqlite 3.25 起支持 RENAME COLUMN，唯一约束与索引会自动跟着改到新列名上
        sqlite3_exec(conn, "ALTER TABLE image RENAME COLUMN img_path TO img_name;", nullptr, nullptr, nullptr);
        sqlite3_exec(conn, "UPDATE image SET img_name = REPLACE(img_name, 'images/', '')"
            " WHERE img_name LIKE 'images/%';", nullptr, nullptr, nullptr);
    }

    /// 外键约束默认是关闭的：不显式打开，建表时写的 ON DELETE CASCADE / SET NULL 全是摆设。
    /// 这个开关不是写进库文件的持久属性，每条连接都必须设一次，且要设在事务之外。
    /// 打开之后两件事由 SQLite 兜住：删分类时它的子孙被 CASCADE 连带删掉；
    /// 删文章时它在 image 表里的记录被连带删掉，不用调用方自己记着收拾。
    void enableForeignKeys()
    {
        if (sqlite3_exec(conn, "PRAGMA foreign_keys = ON;", nullptr, nullptr, nullptr) != SQLITE_OK)
            fatal(L"启用外键约束失败\n\n"
                + std::wstring{ static_cast<const wchar_t*>(sqlite3_errmsg16(conn)) });

        // PRAGMA 执行成功不等于真的开了：编译期去掉外键支持时它是静默无效的，读回来确认一下
        sqlite3_stmt* stmt = nullptr;
        bool on = false;
        if (sqlite3_prepare_v2(conn, "PRAGMA foreign_keys;", -1, &stmt, nullptr) == SQLITE_OK)
        {
            if (sqlite3_step(stmt) == SQLITE_ROW) on = sqlite3_column_int(stmt, 0) != 0;
            sqlite3_finalize(stmt);
        }
        if (!on) fatal(L"SQLite 未启用外键支持\n\n这一版 sqlite3 可能是在关闭外键的情况下编译的");
    }

    void open()
    {
        // 数据目录由 Env::initDataPath 负责创建；db.db 不存在时 sqlite3 会自动创建文件
        auto dbPath = Env::getDataPath() / L"db.db";
        // sqlite3 按 UTF-8 解析路径，这里显式转 UTF-8：既避免 path::string() 的 ANSI 转换
        // 在中文用户名下打不开库，也不用 sqlite3_open16 —— 后者会顺手执行
        // PRAGMA encoding='UTF-16'，把新库的默认编码从 UTF-8 改成 UTF-16le
        auto u8Path = dbPath.u8string();
        auto rc = sqlite3_open(reinterpret_cast<const char*>(u8Path.c_str()), &conn);
        if (rc != SQLITE_OK)
        {
            auto detail = L"无法打开数据库文件\n\n" + dbPath.wstring() + L"\n\n"
                + (conn ? std::wstring{ static_cast<const wchar_t*>(sqlite3_errmsg16(conn)) }
                        : std::wstring{ L"无法创建数据库连接" });
            sqlite3_close(conn);
            fatal(detail);
        }

        createSchema();
        migrateImageTable();
        // 外键放最后开：让建表和旧库迁列这些一次性动作跑在跟升级前一样的环境里，
        // 万一旧库还留着不合外键的历史数据也不会卡住迁移。此后的正常读写都在约束之下
        enableForeignKeys();
    }
}

void Db::init()
{
    open();
}

sqlite3* Db::get()
{
    return conn;
}
