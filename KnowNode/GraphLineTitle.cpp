#include "GraphLineTitle.h"

#include <algorithm>

GraphLineTitle::GraphLineTitle()
{
    label_ = AddChild<dui::Text>();
    OnPointerDown([this](dui::PointerEvent& ev)
    {
        if (ev.button != dui::PointerButton::Left) return;
        if (onClick_)
        {
            onClick_(lineId_);
            ev.handled = true;   // 阻止冒泡到画布的平移逻辑
        }
    });
}

void GraphLineTitle::Refresh(int64_t lineId, const dui::String& title,
                             const dui::Vec2& mid, float font, bool selected)
{
    lineId_ = lineId;
    const float labelW = std::max(28.f, static_cast<float>(title.size()) * font * 1.05f + 12.f);
    const float labelH = font * 1.6f;
    SetStyle(dui::Style()
        .Position(dui::Position::Absolute)
        .Left(mid.x - labelW * 0.5f).Top(mid.y - labelH * 0.5f)
        .Width(labelW).Height(labelH)
        .Background(0xFFFFFFFF).BorderWidth(1.f)
        .BorderColor(selected ? 0xFF1677FF : 0xFFE5E6EB).CornerRadius(3.f)
        .FlexDirection(dui::FlexDirection::Row)
        .JustifyContent(dui::Justify::Center)
        .AlignItems(dui::Align::Center)
        .Cursor(dui::Cursor::Hand)
        .Visible(true));
    label_->SetText(title);
    label_->SetStyle(dui::Style()
        .FontSize(font).Color(0xFF6B7280)
        .TextAlign(dui::TextAlign::Center)
        .TextOverflow(dui::TextOverflow::Ellipsis));
}

void GraphLineTitle::Hide()
{
    lineId_ = 0;
    label_->SetText(L"");
    SetStyle(dui::Style().Position(dui::Position::Absolute)
        .Left(-100000.f).Top(-100000.f).Visible(false));
}
