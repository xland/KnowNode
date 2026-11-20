#include "Category.h"

#include "Db/KnowList.h"

namespace
{
    dui::String ListName(int64_t id)
    {
        KnowListItem item;
        return KnowList::instance().get(id, item) ? dui::Utf8ToWide(item.name.c_str()) : dui::String();
    }

    bool LoadSvgResource(dui::Svg* svg, const wchar_t* resourceName)
    {
        HRSRC res = FindResourceW(nullptr, resourceName, RT_RCDATA);
        if (res == nullptr) return false;
        HGLOBAL mem = LoadResource(nullptr, res);
        if (mem == nullptr) return false;
        const void* data = LockResource(mem);
        const DWORD bytes = SizeofResource(nullptr, res);
        if (data == nullptr || bytes == 0) return false;
        return svg->LoadSvgMemory(data, static_cast<uint32_t>(bytes));
    }
}

Category::Category()
{
    SetStyle(dui::Style()
        .FlexShrink(0.f)
        .FlexDirection(dui::FlexDirection::Column)
        .Background(0xFFFAFAFA)
        .BorderWidth(dui::Thickness(0.f, 0.f, 1.f, 0.f))
        .BorderColor(0xFFE5E6EB));

    auto* head = AddChild<dui::Panel>(dui::Style()
        .Height(28.f).FlexShrink(0.f)
        .FlexDirection(dui::FlexDirection::Row).AlignItems(dui::Align::Center)
        .Padding(dui::Thickness(12.f, 0.f, 8.f, 0.f))
        .Background(0xFFF6F6F6));
    head->AddChild<dui::Text>(L"知识列表", dui::Style()
        .FontSize(13.f).Color(0xFF1F2329).FlexGrow(1.f));
    auto* addList = head->AddChild<dui::Panel>(dui::Style()
        .Width(22.f).Height(22.f)
        .FlexDirection(dui::FlexDirection::Row)
        .JustifyContent(dui::Justify::Center).AlignItems(dui::Align::Center)
        .CornerRadius(4.f).Cursor(dui::Cursor::Hand));
    addList->When(dui::State::Hover, dui::Style().Background(0xFFD6E5F5));
    auto* addIcon = addList->AddChild<dui::Svg>(dui::Style()
        .Width(14.f).Height(14.f));
    LoadSvgResource(addIcon, L"ICON_ADD_LIST");
    addList->OnClick([this](dui::PointerEvent&)
    {
        textDialog_(L"知识名称", L"", [this](const dui::String& name)
        {
            if (name.empty()) return;
            const int64_t id = KnowList::instance().add(dui::WideToUtf8(name));
            if (id != 0) Select(id);
        });
    });

    auto* listScroller = AddChild<dui::Scroller>(dui::Style().FlexGrow(1.f));
    listBody_ = listScroller->AddChild<dui::Panel>(dui::Style()
        .FlexDirection(dui::FlexDirection::Column)
        .Padding(dui::Thickness(0.f, 0.f, 0.f, 4.f)));
}

void Category::Refresh()
{
    const auto items = KnowList::instance().all();
    while (rows_.size() < items.size())
    {
        Row row;
        row.panel = listBody_->AddChild<dui::Panel>(dui::Style()
            .Height(28.f).FlexShrink(0.f)
            .FlexDirection(dui::FlexDirection::Row).AlignItems(dui::Align::Center)
            .Padding(dui::Thickness(0.f, 12.f))
            .Cursor(dui::Cursor::Hand));
        row.panel->When(dui::State::Hover, dui::Style().Background(0xFFE8ECF4));
        row.panel->When(dui::State::Selected, dui::Style().Background(0xFFD3E3FD));
        row.text = row.panel->AddChild<dui::Text>(L"", dui::Style()
            .FontSize(13.f).Color(0xFF1F2329).FlexGrow(1.f)
            .TextOverflow(dui::TextOverflow::Ellipsis));
        row.text->When(dui::State::Selected, dui::Style().Color(0xFF0056B3));
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
        const bool selected = items[i].id == selectedId_;
        row.panel->SetState(dui::State::Selected, selected);
        row.text->SetState(dui::State::Selected, selected);

        const int64_t id = items[i].id;
        row.panel->OnClick([this, id](dui::PointerEvent&) { Select(id); });
        row.panel->OnPointerUp([this, id](dui::PointerEvent& e)
        {
            if (e.button != dui::PointerButton::Right) return;
            dui::Menu menu;
            menu.Add(L"修改知识名称", [this, id] { Rename(id); });
            menu.Add(L"删除知识", [this, id] { Delete(id); });
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
