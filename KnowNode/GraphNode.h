#pragma once

#include <functional>

#include "DUI/DUI.h"

// 节点框与标题文本作为绝对定位元素存在；连线与箭头仍由 GraphView::OnPaint 绘制。
class GraphNode final : public dui::Panel
{
public:
    GraphNode();

    void Refresh(const dui::String& title, const dui::Rect& box,
                 dui::Color fill, dui::Color border, float borderWidth,
                 float font, float zoom, bool hidden);

    // onEdge = 按下点落在节点矩形边缘热区（拉线建关联），否则是节点中部（拖动/双击）。
    using DownFn = std::function<void(GraphNode* node, bool onEdge, dui::PointerEvent& ev)>;
    void OnDown(DownFn fn) { down_ = std::move(fn); }

private:
    bool HandlePointerDown(dui::PointerEvent& ev) override;

    dui::Text* title_ = nullptr;
    float zoom_ = 1.f;
    DownFn down_;
};
