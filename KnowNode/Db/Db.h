#pragma once
#include "../SQLite/sqlite3.h"
#include <functional>
#include <string>
#include <vector>

/**
 * 应用数据库（单例）：持有到 db.db 的持久连接，负责启动时建库建表。
 *
 * - 建表 SQL 由各表类自行注册（registerTable），Db 不关心具体有哪些表；
 * - 具体数据的读写由各表类（KnowList / KnowNode / KnowLine / KnowDetail / Setting）各自对外提供；
 * - 表类都是单例，对外接口是实例方法。
 *
 * 建表一律用 IF NOT EXISTS，重复启动安全。
 */
class Db
{
public:
    /// 一张已注册的表：name 只用于报错定位，create 里执行 CREATE TABLE IF NOT EXISTS ...
    struct Table
    {
        std::string name;
        std::function<void(sqlite3*)> create;
    };

    static Db& instance();

    /// 打开数据目录下的 db.db（不存在则创建）→ 按注册顺序建表 → 开启外键约束。
    /// 打开或建表失败会提示用户并结束进程，不会返回。
    void init();

    /// 关闭连接（进程退出前调用；不调用也能靠进程退出收尾）
    void close();

    /// 取连接：表类通过它执行 SQL
    sqlite3* conn() const { return conn_; }

    /// 表类自注册入口：通常在表类的构造函数里调用
    void registerTable(std::string name, std::function<void(sqlite3*)> create);

    /// 建表 / 建索引这类一次性语句的统一入口：prepare + 执行，任一步失败就带 SQLite 的原因 fatal。
    /// 各表类的 create() 一律走它，别自己拿 DbStmt 只判 ok()——那只代表 SQL 能编译，不代表执行过
    static void execOrFatal(sqlite3* conn, const char* sql, const std::wstring& what);

    /// 数据库不可用时的统一收尾：提示用户并结束进程
    [[noreturn]] static void fatal(const std::wstring& detail);

private:
    Db() = default;
    ~Db();
    Db(const Db&) = delete;
    Db& operator=(const Db&) = delete;

    void open();
    void createTables();
    void enableForeignKeys();

    sqlite3* conn_ = nullptr;
    std::vector<Table> tables_;
};

/// 表类 .cpp 里放一个该类型的文件级静态对象，即可在 main() 之前完成建表注册。
/// 模板参数 T 需提供静态 instance()，且其构造函数内部调用 Db::instance().registerTable(...)。
template <class T>
struct DbTableRegistrar
{
    DbTableRegistrar() { T::instance(); }
};
