#include "GraphNodeAddBtn.h"

GraphNodeAddBtn::GraphNodeAddBtn()
{
    barH_ = AddChild<dui::Panel>();
    barV_ = AddChild<dui::Panel>();
    OnPointerDown([this](dui::PointerEvent& ev)
    {
        if (ev.button != dui::PointerButton::Left) return;
        if (nodeId_ != 0 && onClick_)
        {
            onClick_(nodeId_);
            ev.handled = true;
        }
    });
}

void GraphNodeAddBtn::Refresh(int64_t nodeId, const dui::Vec2& center, float radius, bool visible)
{
    nodeId_ = nodeId;
    if (!visible)
    {
        SetStyle(dui::Style().Position(dui::Position::Absolute)
            .Left(-100000.f).Top(-100000.f)
            .Visible(false).ZIndex(5));
        return;
    }

    // 十字的臂长与线宽按半径等比缩放（原绘制版：半径 9、臂长 4、线宽 1.6）。
    const float arm = radius * (4.f / 9.f);
    const float stroke = radius * (1.6f / 9.f);
    SetStyle(dui::Style()
        .Position(dui::Position::Absolute)
        .Left(center.x - radius).Top(center.y - radius)
        .Width(2.f * radius).Height(2.f * radius)
        .Background(0xFF1677FF).CornerRadius(radius)
        .Visible(true).ZIndex(5).Cursor(dui::Cursor::Hand));
    barH_->SetStyle(dui::Style().Position(dui::Position::Absolute)
        .Left(radius - arm).Top(radius - stroke * 0.5f)
        .Width(2.f * arm).Height(stroke).Background(0xFFFFFFFF));
    barV_->SetStyle(dui::Style().Position(dui::Position::Absolute)
        .Left(radius - stroke * 0.5f).Top(radius - arm)
        .Width(stroke).Height(2.f * arm).Background(0xFFFFFFFF));
}
