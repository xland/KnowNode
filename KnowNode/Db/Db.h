#pragma once
#include "../SQLite/sqlite3.h"

/**
 * 应用数据库：持有到 db.db 的持久连接（Db.cpp 里的文件级静态变量），负责启动时建库建表。
 * 建表使用 IF NOT EXISTS，因此重复启动安全（不重复建）。
 * 这里只管连接与 schema，具体数据的读写由 Category / Article 各自对外提供。
 */
namespace Db
{
	/// 打开数据目录下的 db.db；文件不存在则创建，并执行建表 SQL，
	/// 打开或建表失败时直接提示用户并退出进程，不会返回；
	/// 表都建好后写入分类与文章的测试数据（由 Category / Article 各自负责）
	void init();
	/// 取数据库连接；Category / Article 通过它执行 SQL
	sqlite3* get();
}
