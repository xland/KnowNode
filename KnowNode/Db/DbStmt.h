#pragma once
#include "../SQLite/sqlite3.h"
#include <string>

/**
 * sqlite3_stmt 的轻量包装：构造时 prepare，析构时 finalize。
 * 表类里的 SQL 一律走它，省得每处都写一遍 prepare/bind/step/finalize 的样板。
 */
class DbStmt
{
public:
    DbStmt(sqlite3* conn, const char* sql)
    {
        ok_ = sqlite3_prepare_v2(conn, sql, -1, &stmt_, nullptr) == SQLITE_OK;
    }
    ~DbStmt()
    {
        if (stmt_) sqlite3_finalize(stmt_);
    }
    DbStmt(const DbStmt&) = delete;
    DbStmt& operator=(const DbStmt&) = delete;

    bool ok() const { return ok_ && stmt_ != nullptr; }
    sqlite3_stmt* raw() const { return stmt_; }

    void bindInt(int idx, sqlite3_int64 value) { sqlite3_bind_int64(stmt_, idx, value); }
    void bindDouble(int idx, double value) { sqlite3_bind_double(stmt_, idx, value); }
    void bindText(int idx, const std::string& value)
    {
        sqlite3_bind_text(stmt_, idx, value.data(), static_cast<int>(value.size()), SQLITE_TRANSIENT);
    }

    /// 执行并期望取到一行；返回 true 表示有行
    bool step() { return sqlite3_step(stmt_) == SQLITE_ROW; }
    /// 只执行不需要结果的语句
    void run() { sqlite3_step(stmt_); }
    /// 执行不需要结果的语句并检查有没有真的跑完（CREATE / INSERT / UPDATE / DELETE 都用它）。
    /// ok() 只代表 prepare 成功——建表语句光 prepare 不 step 等于什么都没建，
    /// 下一句引用该表的 CREATE INDEX 就会因 "no such table" 在 prepare 阶段失败
    bool exec()
    {
        auto rc = sqlite3_step(stmt_);
        return rc == SQLITE_DONE || rc == SQLITE_OK;
    }

    sqlite3_int64 columnInt(int col) const { return sqlite3_column_int64(stmt_, col); }
    double columnDouble(int col) const { return sqlite3_column_double(stmt_, col); }
    /// 空值与 NULL 都返回空串
    std::string columnText(int col) const
    {
        auto text = reinterpret_cast<const char*>(sqlite3_column_text(stmt_, col));
        return text ? std::string(text) : std::string();
    }

private:
    sqlite3_stmt* stmt_ = nullptr;
    bool ok_ = false;
};
