#include "StatusBar.h"

#include <string>

StatusBar::StatusBar()
{
    SetStyle(dui::Style()
        .Height(28.f).FlexShrink(0.f)
        .FlexDirection(dui::FlexDirection::Row).AlignItems(dui::Align::Center)
        .Padding(dui::Thickness(0.f, 10.f)).Background(0xFFF1F2F4));

    name_ = AddChild<dui::Text>(L"未选中任何内容", dui::Style()
        .FontSize(12.f).Color(0xFF6B7280).FlexGrow(1.f));
    count_ = AddChild<dui::Text>(L"", dui::Style()
        .FontSize(12.f).Color(0xFF6B7280));

    On<StatusSelectionEvent>(kStatusSelectionEvent, [this](const StatusSelectionEvent& e)
    {
        switch (e.kind)
        {
        case StatusSelectionKind::Node: SetSelectedNode(e.title); break;
        case StatusSelectionKind::Line: SetSelectedLine(e.title); break;
        default:                        ClearSelection(); break;
        }
    });
}

void StatusBar::SetSelectedNode(const dui::String& name)
{
    name_->SetText(name.empty() ? L"未选中任何内容" : L"选中节点：" + name);
}

void StatusBar::SetSelectedLine(const dui::String& name)
{
    name_->SetText(name.empty() ? L"未选中任何内容" : L"选中连线：" + name);
}

void StatusBar::ClearSelection()
{
    name_->SetText(L"未选中任何内容");
}

void StatusBar::SetNodeCount(size_t count)
{
    count_->SetText(L"节点 " + std::to_wstring(count));
}
