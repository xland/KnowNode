#include "GraphNode.h"

#include <algorithm>

GraphNode::GraphNode()
{
    title_ = AddChild<dui::Text>();
}

void GraphNode::Refresh(const dui::String& title, const dui::Rect& box,
                        dui::Color fill, dui::Color border, float borderWidth,
                        float font, float zoom, bool hidden)
{
    zoom_ = zoom;
    SetStyle(dui::Style()
        .Position(dui::Position::Absolute)
        .Left(box.x).Top(box.y).Width(box.w).Height(box.h)
        .Background(fill)
        .BorderWidth(borderWidth).BorderColor(border)
        .CornerRadius(3.f * zoom)
        .FlexDirection(dui::FlexDirection::Row)
        .JustifyContent(dui::Justify::Center)
        .AlignItems(dui::Align::Center)
        .Display(hidden ? dui::Display::None : dui::Display::Flex));
    title_->SetText(title);
    title_->SetStyle(dui::Style()
        .FontSize(font).Color(0xFF1F2329)
        .TextAlign(dui::TextAlign::Center)
        .TextOverflow(dui::TextOverflow::Ellipsis));
}

bool GraphNode::HandlePointerDown(dui::PointerEvent& ev)
{
    if (ev.button != dui::PointerButton::Left)
        return false;   // 右键冒泡到 GraphView 做上下文菜单

    const dui::Rect b = Bounds();
    const float px = ev.pos.x - b.x;
    const float py = ev.pos.y - b.y;
    const float band = 6.f * zoom_;
    const float dx = std::min(px, b.w - px);
    const float dy = std::min(py, b.h - py);
    const bool onEdge = std::min(dx, dy) <= band;

    if (down_) down_(this, onEdge, ev);
    ev.handled = true;
    return true;
}
