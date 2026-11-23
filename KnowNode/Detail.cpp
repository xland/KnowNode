#include "Detail.h"

#include "Db/KnowDetail.h"
#include "IconFont.h"
#include "RichDocStore.h"

namespace
{
    namespace rt = dui::richtext;

    constexpr float kSplitterWidth = 4.f;
    constexpr float kHandleWidth = 14.f;
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
    // 默认折叠态 → 展开按钮（向左箭头）。收起/展开时由 ApplyLayout 切换码位。
    arrow_ = IconFont::CreateIcon(handle_, IconFont::ArrowLeft, dui::Style()
        .Width(14.f).Height(14.f).FontSize(13.f).Color(0xFF4C4F54)
        .Cursor(dui::Cursor::Hand));
    handle_->OnClick([this](dui::PointerEvent&) { Toggle(); });

    splitter_ = AddChild<Splitter>(dui::Style()
        .Width(kSplitterWidth).FlexShrink(0.f)
        .Background(0xFFEEEEEE)
        .Cursor(dui::Cursor::SizeWE));
    splitter_->When(dui::State::Hover, dui::Style().Background(0xFFdddddd));
    splitter_->OnDragStart([this] { dragBase_ = width_; });
    splitter_->OnDrag([this](float dx) { width_ = dragBase_ - dx; if (onResize_) onResize_(); });
    splitter_->OnDragEnd([this] { if (onWidthCommitted_) onWidthCommitted_(); });

    panel_ = AddChild<dui::Panel>(dui::Style()
        .Width(width_).FlexShrink(0.f)
        .FlexDirection(dui::FlexDirection::Column)
        .Background(0xFFFFFFFF));

    auto* head = panel_->AddChild<dui::Panel>(dui::Style()
        .Height(28.f).FlexShrink(0.f)
        .FlexDirection(dui::FlexDirection::Row).AlignItems(dui::Align::Center)
        .Padding(dui::Thickness(12.f, 0.f, 8.f, 0.f))
        .Background(0xFFF6F6F6));
    title_ = head->AddChild<dui::Text>(L"", dui::Style()
        .FontSize(13.f).Color(0xFF1F2329).FlexGrow(1.f)
        .TextOverflow(dui::TextOverflow::Ellipsis));

    auto* toolbar = panel_->AddChild<dui::Panel>(dui::Style()
        .FlexDirection(dui::FlexDirection::Row)
        .FlexWrap(true).Gap(0.f).Padding(dui::Thickness(0.f))
        .FlexShrink(0.f).Background(0xFFFAFAFA));   // 上下左右内边距全 0：高度 = 按钮高度，首钮贴左边

    // 工具按钮：无边框、无圆角、常态透明、紧密排列；仅 hover 时浮出一层浅灰底。
    auto boxButton = [toolbar]() -> dui::Panel*
    {
        auto* b = toolbar->AddChild<dui::Panel>(dui::Style()
            .Width(32.f).Height(30.f)
            .FlexDirection(dui::FlexDirection::Row)
            .JustifyContent(dui::Justify::Center).AlignItems(dui::Align::Center)
            .Cursor(dui::Cursor::Hand));
        b->When(dui::State::Hover, dui::Style().Background(0xFFEFF1F6));
        return b;
    };
    auto iconTool = [boxButton](wchar_t glyph, auto fn)
    {
        auto* b = boxButton();
        IconFont::CreateIcon(b, glyph, dui::Style()
            .Width(18.f).Height(18.f).FontSize(16.f).Color(0xFF6B7280));
        b->OnClick([fn](dui::PointerEvent&) { fn(); });
    };

    // 行内格式（对当前选区切换，需先选中文字才生效）
    iconTool(IconFont::Bold, [this] { if (editor_->ToggleBold()) MarkDirty(); });
    iconTool(IconFont::Italic, [this] { if (editor_->ToggleItalic()) MarkDirty(); });
    iconTool(IconFont::Underline, [this] { if (editor_->ToggleUnderline()) MarkDirty(); });
    iconTool(IconFont::Strike, [this] { if (editor_->ToggleStrike()) MarkDirty(); });
    // 块级：分割线 + 原有四项
    iconTool(IconFont::HorizonLine, [this] { AppendBlock(rt::BlockKind::Rule); });
    iconTool(IconFont::ListBullet, [this] { AppendBlock(rt::BlockKind::List); });
    iconTool(IconFont::Quote, [this] { AppendBlock(rt::BlockKind::Quote); });
    iconTool(IconFont::Code, [this] { AppendBlock(rt::BlockKind::Code); });
    iconTool(IconFont::Image, [this] { InsertImageFile(); });

    scroller_ = panel_->AddChild<dui::Scroller>(dui::Style().FlexGrow(1.f));
    editor_ = scroller_->AddChild<dui::RichEditor>(dui::Style()
        .FlexGrow(1.f).Padding(dui::Thickness(12.f)).Background(0xFFFFFFFF));
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
    if (detailId != 0) json = db::KnowDetail::instance().content(detailId);
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
        db::KnowDetail::instance().save(detailId_, dui::WideToUtf8(json));
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
    case rt::BlockKind::Rule:
        block.kind = rt::BlockKind::Rule;   // 水平分割线（框架已有类型，无行内内容）
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
    IconFont::SetGlyph(arrow_, expanded_ ? IconFont::ArrowRight : IconFont::ArrowLeft);
    splitter_->Set(dui::Prop::Width, expanded_ ? kSplitterWidth : 0.f);
    splitter_->Set(dui::Prop::Visible, expanded_ ? 1u : 0u);
    panel_->Set(dui::Prop::Width, expanded_ ? width_ : 0.f);
    panel_->Set(dui::Prop::Visible, expanded_ ? 1u : 0u);
}
