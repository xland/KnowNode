#include "Category.h"

#include "Db/KnowList.h"

namespace
{
    dui::String ListName(int64_t id)
    {
        KnowListItem item;
        return KnowList::instance().get(id, item) ? dui::Utf8ToWide(item.name.c_str()) : dui::String();
    }
}

Category::Category()
{
    SetStyle(dui::Style()
        .FlexShrink(0.f)
        .FlexDirection(dui::FlexDirection::Column)
        .Background(0xFFF7F8FA));

    auto* head = AddChild<dui::Panel>(dui::Style()
        .Height(42.f).FlexShrink(0.f)
        .FlexDirection(dui::FlexDirection::Row).AlignItems(dui::Align::Center)
        .Padding(dui::Thickness(0.f, 10.f)));
    head->AddChild<dui::Text>(L"知识", dui::Style()
        .FontSize(13.f).Color(0xFF4B5563).FlexGrow(1.f));
    auto* addList = head->AddChild<dui::Button>(L"＋", dui::Style()
        .Width(28.f).Height(28.f).FontSize(18.f).Color(0xFF2563EB)
        .TextAlign(dui::TextAlign::Center).Cursor(dui::Cursor::Hand));
    addList->OnClick([this](dui::PointerEvent&)
    {
        textDialog_(L"新知识名称", L"", [this](const dui::String& name)
        {
            if (name.empty()) return;
            const int64_t id = KnowList::instance().add(dui::WideToUtf8(name));
            if (id != 0) Select(id);
        });
    });

    auto* listScroller = AddChild<dui::Scroller>(dui::Style().FlexGrow(1.f));
    listBody_ = listScroller->AddChild<dui::Panel>(dui::Style()
        .FlexDirection(dui::FlexDirection::Column)
        .Padding(dui::Thickness(6.f, 6.f)));
}

void Category::Refresh()
{
    const auto items = KnowList::instance().all();
    while (rows_.size() < items.size())
    {
        Row row;
        row.panel = listBody_->AddChild<dui::Panel>(dui::Style()
            .Height(34.f).FlexShrink(0.f)
            .FlexDirection(dui::FlexDirection::Row).AlignItems(dui::Align::Center)
            .Padding(dui::Thickness(0.f, 10.f)).CornerRadius(4.f)
            .Cursor(dui::Cursor::Hand));
        row.panel->When(dui::State::Hover, dui::Style().Background(0xFFEEF2FF));
        row.panel->When(dui::State::Selected, dui::Style().Background(0xFFDBEAFE));
        row.text = row.panel->AddChild<dui::Text>(L"", dui::Style()
            .FontSize(13.f).Color(0xFF1F2933).FlexGrow(1.f)
            .TextOverflow(dui::TextOverflow::Ellipsis));
        rows_.push_back(row);
    }

    for (size_t i = 0; i < rows_.size(); ++i)
    {
        Row& row = rows_[i];
        if (i >= items.size())
        {
            row.panel->Set(dui::Prop::Display, static_cast<uint32_t>(dui::Display::None));
            continue;
        }
        row.panel->Set(dui::Prop::Display, static_cast<uint32_t>(dui::Display::Flex));
        row.text->SetText(dui::Utf8ToWide(items[i].name.c_str()));
        row.panel->SetState(dui::State::Selected, items[i].id == selectedId_);

        const int64_t id = items[i].id;
        row.panel->OnClick([this, id](dui::PointerEvent&) { Select(id); });
        row.panel->OnPointerUp([this, id](dui::PointerEvent& e)
        {
            if (e.button != dui::PointerButton::Right) return;
            dui::Menu menu;
            menu.Add(L"重命名", [this, id] { Rename(id); });
            menu.Add(L"删除", [this, id] { Delete(id); });
            menuPopup_(menu, e.pos);
            e.handled = true;
        });
    }
}

void Category::Select(int64_t id)
{
    selectedId_ = id;
    Refresh();
    if (onSelect_) onSelect_(id);
}

void Category::Rename(int64_t id)
{
    textDialog_(L"知识名称", ListName(id), [this, id](const dui::String& text)
    {
        if (text.empty() || text.size() > 36) return;
        KnowList::instance().rename(id, dui::WideToUtf8(text));
        Refresh();
        if (onRenamed_) onRenamed_(id, text);
    });
}

void Category::Delete(int64_t id)
{
    KnowList::instance().remove(id);
    EnsureDefaultList();
    const auto rest = KnowList::instance().all();
    Select(rest.empty() ? 0 : rest.front().id);
}

void Category::EnsureDefaultList()
{
    if (KnowList::instance().all().empty())
        KnowList::instance().add("默认知识");
}
