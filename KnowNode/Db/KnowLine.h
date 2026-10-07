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
 * color 是用户在右键菜单里挑的标记色（0 = 未着色，用默认样式）。
 */
struct KnowLineItem
{
    int64_t id = 0;
    int64_t nodeAId = 0;
    int64_t nodeBId = 0;
    int64_t detailId = 0;
    int64_t createdAt = 0;
    int64_t updatedAt = 0;
    int color = 0;
};

class KnowLine
{
public:
    /// 标记色的数量：0 = 未着色，1..maxColor 是具体的颜色，与前端那组色值一一对应
    static constexpr int maxColor = 6;

    static KnowLine& instance();

    /// 建表（由 Db::init 调用）
    void create(sqlite3* conn);

    /// 建立关联（同时建一条空详情），返回连线 id。
    /// 两端不属于同一个知识、或节点不存在时返回 0；已存在这条关联时返回既有 id。
    int64_t add(int64_t nodeAId, int64_t nodeBId);
    /// 删除连线：连带删除它的详情
    bool remove(int64_t id);
    /// 改标记色（超出 [0, maxColor] 的值会被夹到边界上）
    bool setColor(int64_t id, int color);

    /// 某个知识的全部连线
    std::vector<KnowLineItem> ofList(int64_t listId) const;
    /// 取单条连线；找不到返回 false
    bool get(int64_t id, KnowLineItem& out) const;

private:
    KnowLine();
};
