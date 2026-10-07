#pragma once
#include "../SQLite/sqlite3.h"
#include <cstdint>
#include <string>
#include <vector>

/**
 * know_list 表：一个个「知识」（历史知识、编程知识、文学知识……），
 * 每一项是一张网的入口。
 *
 * 知识自己也有一份详情（`detail_id` → know_detail），写的是这个知识**整体的描述**
 * （与节点 / 连线的详情同一张表、同一套读写，见 Arch/40）。
 */
struct KnowListItem
{
    int64_t id = 0;
    std::string name;
    int64_t detailId = 0;
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

    /**
     * 这条知识的详情 id：没有就补建一条挂上去（老库的 detail_id 是后加的列，旧行是 0）。
     * 返回 0 表示失败（知识不存在或建详情失败）。
     */
    int64_t ensureDetail(int64_t id);

private:
    KnowList();
};
