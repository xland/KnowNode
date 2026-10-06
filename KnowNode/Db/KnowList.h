#pragma once
#include "../SQLite/sqlite3.h"
#include <cstdint>
#include <string>
#include <vector>

/**
 * know_list 表：一个个「知识」（历史知识、编程知识、文学知识……），
 * 每一项是一张网的入口。
 */
struct KnowListItem
{
    int64_t id = 0;
    std::string name;
    int64_t createdAt = 0;
    int64_t updatedAt = 0;
};

class KnowList
{
public:
    static KnowList& instance();

    /// 建表（由 Db::init 调用）
    void create(sqlite3* conn);

    /// 新建一个知识，返回新 id；失败返回 0
    int64_t add(const std::string& name);
    bool rename(int64_t id, const std::string& name);
    /// 删除知识：连带删除它的节点、连线，以及这些节点/连线各自的详情
    bool remove(int64_t id);

    /// 全部知识，按创建时间升序
    std::vector<KnowListItem> all() const;
    /// 取单个知识；找不到返回 false
    bool get(int64_t id, KnowListItem& out) const;

private:
    KnowList();
};
