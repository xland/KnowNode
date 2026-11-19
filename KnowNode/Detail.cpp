#include "Detail.h"

#include "DUI/UI/Svg.h"

#include "Db/KnowDetail.h"
#include "RichDocStore.h"

namespace
{
    namespace rt = dui::richtext;

    constexpr float kSplitterWidth = 6.f;
    constexpr float kHandleWidth = 14.f;

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

    void AddSvgIcon(dui::Panel* parent, const wchar_t* resourceName, float size)
    {
        auto* svg = parent->AddChild<dui::Svg>(dui::Style()
            .Width(size).Height(size).Padding(dui::Thickness(2.f)));
        LoadSvgResource(svg, resourceName);
    }
}

Detail::Detail()
{
    SetStyle(dui::Style().FlexDirection(dui::FlexDirection::Row).FlexShrink(0.f));

    handle_ = AddChild<dui::Panel>(dui::Style()
        .Width(kHandleWidth).FlexShrink(0.f)
        .FlexDirection(dui::FlexDirection::Row)
        .JustifyContent(dui::Justify::Center).AlignItems(dui::Align::Center)
        .Background(0xFFF1F2F4).Cursor(dui::Cursor::Hand));
    handle_->When(dui::State::Hover, dui::Style().Background(0xFFE5E7EB));
    arrow_ = handle_->AddChild<dui::Text>(L"▶", dui::Style()
        .FontSize(12.f).Color(0xFF6B7280).TextAlign(dui::TextAlign::Center));
    handle_->OnClick([this](dui::PointerEvent&) { Toggle(); });

    splitter_ = AddChild<Splitter>(dui::Style()
        .Width(kSplitterWidth).FlexShrink(0.f)
        .Cursor(dui::Cursor::SizeWE));
    splitter_->When(dui::State::Hover, dui::Style().Background(0xFFF2F6F8));
    splitter_->When(dui::State::Pressed, dui::Style().Background(0xFFF2F6F8));
    splitter_->OnDragStart([this] { dragBase_ = width_; });
    splitter_->OnDrag([this](float dx) { width_ = dragBase_ - dx; if (onResize_) onResize_(); });
    splitter_->OnDragEnd([this] { if (onWidthCommitted_) onWidthCommitted_(); });

    panel_ = AddChild<dui::Panel>(dui::Style()
        .Width(width_).FlexShrink(0.f)
        .FlexDirection(dui::FlexDirection::Column)
        .Background(0xFFFFFFFF));

    auto* head = panel_->AddChild<dui::Panel>(dui::Style()
        .Height(42.f).FlexShrink(0.f)
        .FlexDirection(dui::FlexDirection::Row).AlignItems(dui::Align::Center)
        .Padding(dui::Thickness(0.f, 12.f)));
    title_ = head->AddChild<dui::Text>(L"", dui::Style()
        .FontSize(13.f).Color(0xFF111827).FlexGrow(1.f)
        .TextOverflow(dui::TextOverflow::Ellipsis));
    auto* saveBtn = head->AddChild<dui::Button>(L"保存", dui::Style()
        .FontSize(13.f).Color(0xFF2563EB).Padding(dui::Thickness(4.f, 8.f))
        .Cursor(dui::Cursor::Hand));
    saveBtn->OnClick([this](dui::PointerEvent&) { Flush(); });

    auto* toolbar = panel_->AddChild<dui::Panel>(dui::Style()
        .FlexDirection(dui::FlexDirection::Row)
        .FlexWrap(true).Gap(4.f).Padding(dui::Thickness(6.f, 10.f))
        .FlexShrink(0.f).Background(0xFFFAFAFA));

    auto textTool = [toolbar](const wchar_t* label, auto fn)
    {
        auto* b = toolbar->AddChild<dui::Button>(label, dui::Style()
            .FontSize(12.f).Color(0xFF374151)
            .Padding(dui::Thickness(5.f, 8.f))
            .Background(0xFFFFFFFF).BorderWidth(1.f).BorderColor(0xFFE5E7EB)
            .CornerRadius(4.f).Cursor(dui::Cursor::Hand));
        b->OnClick([fn](dui::PointerEvent&) { fn(); });
    };
    auto iconTool = [toolbar](const wchar_t* resourceName, auto fn)
    {
        auto* b = toolbar->AddChild<dui::Panel>(dui::Style()
            .Width(34.f).Height(28.f)
            .FlexDirection(dui::FlexDirection::Row)
            .JustifyContent(dui::Justify::Center).AlignItems(dui::Align::Center)
            .Background(0xFFFFFFFF).BorderWidth(1.f).BorderColor(0xFFE5E7EB)
            .CornerRadius(4.f).Cursor(dui::Cursor::Hand));
        b->When(dui::State::Hover, dui::Style().Background(0xFFF3F4F6));
        AddSvgIcon(b, resourceName, 18.f);
        b->OnClick([fn](dui::PointerEvent&) { fn(); });
    };
    textTool(L"标题", [this] { AppendBlock(rt::BlockKind::Heading); });
    textTool(L"正文", [this] { AppendBlock(rt::BlockKind::Paragraph); });
    iconTool(L"ICON_LIST_BULLET", [this] { AppendBlock(rt::BlockKind::List); });
    iconTool(L"ICON_QUOTE", [this] { AppendBlock(rt::BlockKind::Quote); });
    iconTool(L"ICON_CODE", [this] { AppendBlock(rt::BlockKind::Code); });
    iconTool(L"ICON_IMAGE", [this] { InsertImageFile(); });

    auto* scroller = panel_->AddChild<dui::Scroller>(dui::Style().FlexGrow(1.f));
    auto* paper = scroller->AddChild<dui::Panel>(dui::Style()
        .Padding(dui::Thickness(18.f)).Background(0xFFFFFFFF));
    editor_ = paper->AddChild<dui::RichEditor>();
    editor_->OnKeyDown([this](dui::KeyEvent&) { MarkDirty(); });
    editor_->OnChar([this](dui::KeyEvent&) { MarkDirty(); });
    editor_->OnPointerUp([this](dui::PointerEvent&) { MarkDirty(); });
    editor_->OnDropFiles([this](dui::DropFilesEvent&) { MarkDirty(); });

    ApplyLayout();
}

void Detail::Show(Kind kind, int64_t detailId, const dui::String& title)
{
    Flush();
    kind_ = kind;
    detailId_ = detailId;
    title_->SetText(title);
    LoadDoc(detailId);
    editor_->SetDoc(doc_);
    editor_->SetEditable(detailId != 0);
    lastSavedJson_ = RichDocStore::Save(doc_);
    dirty_ = false;
}

void Detail::Clear()
{
    kind_ = Kind::None;
    detailId_ = 0;
}

void Detail::SetTitle(const dui::String& title)
{
    title_->SetText(title);
}

void Detail::UpdateTitleIf(Kind kind, int64_t detailId, const dui::String& title)
{
    if (kind_ == kind && detailId_ == detailId) title_->SetText(title);
}

void Detail::LoadDoc(int64_t detailId)
{
    std::string json;
    if (detailId != 0) json = KnowDetail::instance().content(detailId);
    RichDocStore::Load(json, doc_);
}

void Detail::MarkDirty()
{
    dirty_ = true;
}

void Detail::Flush()
{
    if (!dirty_ || detailId_ == 0 || editor_ == nullptr) return;
    const dui::String json = RichDocStore::Save(editor_->Doc());
    if (json != lastSavedJson_)
    {
        KnowDetail::instance().save(detailId_, dui::WideToUtf8(json));
        lastSavedJson_ = json;
        doc_ = editor_->Doc();
    }
    dirty_ = false;
}

void Detail::AppendBlock(rt::BlockKind kind)
{
    if (detailId_ == 0) return;
    rt::RichDoc doc = editor_->Doc();
    rt::Block block;

    switch (kind)
    {
    case rt::BlockKind::Heading:
        block.kind = rt::BlockKind::Heading;
        block.level = 2;
        block.runs.push_back(rt::Run{ L"标题", {} });
        break;
    case rt::BlockKind::List:
    {
        block.kind = rt::BlockKind::List;
        block.listType = rt::ListType::Unordered;
        rt::Block item;
        item.kind = rt::BlockKind::ListItem;
        item.runs.push_back(rt::Run{ L"", {} });
        block.children.push_back(std::move(item));
        break;
    }
    case rt::BlockKind::Code:
        block.kind = rt::BlockKind::Code;
        block.showLineNumbers = true;
        break;
    case rt::BlockKind::Quote:
        block.kind = rt::BlockKind::Quote;
        block.runs.push_back(rt::Run{ L"", {} });
        break;
    default:
        block.kind = rt::BlockKind::Paragraph;
        block.runs.push_back(rt::Run{ L"", {} });
        break;
    }

    doc.blocks.push_back(std::move(block));
    editor_->SetDoc(doc);
    doc_ = editor_->Doc();
    MarkDirty();
}

void Detail::InsertImageFile()
{
    if (detailId_ == 0) return;
    dui::String path;
    if (pickImage_ && pickImage_(path) && editor_->InsertImage(path)) MarkDirty();
}

void Detail::Toggle()
{
    if (!hasKnowledge_) return;
    if (expanded_) Flush();
    expanded_ = !expanded_;
    ApplyLayout();
    if (onToggle_) onToggle_();
}

void Detail::SetExpanded(bool expanded)
{
    expanded_ = expanded;
    ApplyLayout();
}

void Detail::SetHasKnowledge(bool has)
{
    hasKnowledge_ = has;
    ApplyLayout();
}

void Detail::SetWidth(float width)
{
    width_ = width;
    ApplyLayout();
}

void Detail::ApplyLayout()
{
    handle_->Set(dui::Prop::Width, hasKnowledge_ ? kHandleWidth : 0.f);
    handle_->Set(dui::Prop::Visible, hasKnowledge_ ? 1u : 0u);
    arrow_->SetText(expanded_ ? L"◀" : L"▶");
    splitter_->Set(dui::Prop::Width, expanded_ ? kSplitterWidth : 0.f);
    splitter_->Set(dui::Prop::Visible, expanded_ ? 1u : 0u);
    panel_->Set(dui::Prop::Width, expanded_ ? width_ : 0.f);
    panel_->Set(dui::Prop::Visible, expanded_ ? 1u : 0u);
}
