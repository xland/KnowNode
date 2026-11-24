#pragma once

#include <functional>

#include "DUI/DUI.h"

// 选中节点右侧的加号：蓝圆底 + 白色十字，整体是一个绝对定位元素。
// 位置与源节点由 GraphView 给出，点击意图以 nodeId 回调。
class GraphNodeAddBtn final : public dui::Panel
{
public:
    GraphNodeAddBtn();

    void Refresh(int64_t nodeId, const dui::Vec2& center, float radius, bool visible);

    using ClickFn = std::function<void(int64_t nodeId)>;
    void OnClick(ClickFn fn) { onClick_ = std::move(fn); }

private:
    dui::Panel* barH_ = nullptr;
    dui::Panel* barV_ = nullptr;
    int64_t nodeId_ = 0;
    ClickFn onClick_;
};
