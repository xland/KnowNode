#pragma once
#include "../SQLite/sqlite3.h"
#include <cstdint>
#include <string>
#include <vector>

/**
 * know_node 表：知识内部的节点（一个知识点）。
 * 节点只属于一个知识（list_id）；坐标 x/y 由用户在画布上拖拽摆放后持久化。
 * color 是用户在右键菜单里挑的标记色（0 = 未着色，用默认样式）。
 */
struct KnowNodeItem
{
    int64_t id = 0;
    int64_t listId = 0;
    std::string title;
    int64_t detailId = 0;
    double x = 0;
    double y = 0;
    int64_t createdAt = 0;
    int64_t updatedAt = 0;
    int color = 0;
};

class KnowNode
{
public:
    /// 新建节点的默认标题
    static constexpr const char* defaultTitle = "未命名";
    /// 标记色的数量：0 = 未着色，1..maxColor 是具体的颜色，与前端那组色值一一对应
    static constexpr int maxColor = 6;

    static KnowNode& instance();

    /// 建表（由 Db::init 调用）
    void create(sqlite3* conn);

    /// 新建节点（同时建一条空详情），返回新 id；失败返回 0
    int64_t add(int64_t listId, double x, double y, const std::string& title = defaultTitle);
    /// 改标题
    bool updateTitle(int64_t id, const std::string& title);
    /// 拖拽后保存坐标
    bool move(int64_t id, double x, double y);
    /// 改标记色（超出 [0, maxColor] 的值会被夹到边界上）
    bool setColor(int64_t id, int color);
    /// 删除节点：连带删除它的所有连线，以及它和这些连线各自的详情
    bool remove(int64_t id);

    /// 某个知识的全部节点
    std::vector<KnowNodeItem> ofList(int64_t listId) const;
    /// 取单个节点；找不到返回 false
    bool get(int64_t id, KnowNodeItem& out) const;
    /// 取节点所属的知识 id；找不到返回 0（供连线校验两端是否同属一个知识）
    int64_t listIdOf(int64_t id) const;

private:
    KnowNode();
};
