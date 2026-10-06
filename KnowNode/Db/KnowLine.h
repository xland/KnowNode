#pragma once
#include "../SQLite/sqlite3.h"
#include <cstdint>
#include <string>
#include <vector>

/**
 * know_line 表：知识点之间的关联（连线）。
 * 关联是**无向**的：存的时候把小的 id 放 node_a_id、大的放 node_b_id，
 * 于是 A-B 与 B-A 是同一条，由 UNIQUE 保证不重复。
 * 连线不跨知识：两端节点必须属于同一个 know_list。
 */
struct KnowLineItem
{
    int64_t id = 0;
    int64_t nodeAId = 0;
    int64_t nodeBId = 0;
    int64_t detailId = 0;
    int64_t createdAt = 0;
    int64_t updatedAt = 0;
};

class KnowLine
{
public:
    static KnowLine& instance();

    /// 建表（由 Db::init 调用）
    void create(sqlite3* conn);

    /// 建立关联（同时建一条空详情），返回连线 id。
    /// 两端不属于同一个知识、或节点不存在时返回 0；已存在这条关联时返回既有 id。
    int64_t add(int64_t nodeAId, int64_t nodeBId);
    /// 删除连线：连带删除它的详情
    bool remove(int64_t id);

    /// 某个知识的全部连线
    std::vector<KnowLineItem> ofList(int64_t listId) const;
    /// 取单条连线；找不到返回 false
    bool get(int64_t id, KnowLineItem& out) const;

private:
    KnowLine();
};
