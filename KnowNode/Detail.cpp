#include "Detail.h"

#include "Db/KnowDetail.h"
#include "IconFont.h"
#include "RichDocStore.h"

namespace
{
    namespace rt = dui::richtext;

    constexpr float kSplitterWidth = 4.f;
}

Detail::Detail()
{
    SetStyle(dui::Style().FlexDirection(dui::FlexDirection::Row).FlexShrink(0.f));

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
        .Height(36.f).FlexShrink(0.f)
        .FlexDirection(dui::FlexDirection::Row).AlignItems(dui::Align::Center)
        .Background(0xFFF6F6F6));
    title_ = head->AddChild<dui::TextBox>(dui::Style()
        .Height(36.f).FontSize(13.f).Color(0xFF1F2329).FlexGrow(1.f)
        .Margin(dui::Thickness(0.f)).Padding(dui::Thickness(0.f, 8.f))
        .BorderWidth(0.f).Background(0xFFFFFFFF));
    title_->OnSubmit([this] { CommitTitle(); });

    // 连线箭头方向栏：三个互斥单选（Radio 图标 + 文字），仅连线时显示。
    dirBar_ = panel_->AddChild<dui::Panel>(dui::Style()
        .Height(38.f).FlexShrink(0.f)
        .FlexDirection(dui::FlexDirection::Row).AlignItems(dui::Align::Center)
        .Padding(dui::Thickness(12.f, 0.f, 8.f, 0.f)).Gap(16.f)
        .Background(0xFFFFFFFF)
        .Display(dui::Display::None));
    const wchar_t* dirTexts[3] = { L"正向", L"反向", L"双向" };
    for (int i = 0; i < 3; ++i)
    {
        auto* opt = dirBar_->AddChild<dui::Panel>(dui::Style()
            .FlexDirection(dui::FlexDirection::Row).AlignItems(dui::Align::Center)
            .Gap(4.f).Cursor(dui::Cursor::Hand).FlexShrink(0.f));
        dirRadios_[i] = opt->AddChild<dui::Icon>(dui::Style()
            .Width(14.f).Height(14.f).Color(0xFF1677FF));
        dirLabels_[i] = opt->AddChild<dui::Text>(dirTexts[i], dui::Style()
            .FontSize(12.f).Color(0xFF1F2329));
        opt->OnClick([this, i](dui::PointerEvent&) { SetDirection(i); });
    }

    auto* toolbar = panel_->AddChild<dui::Panel>(dui::Style()
        .FlexDirection(dui::FlexDirection::Row)
        .FlexWrap(true).Gap(0.f).Padding(dui::Thickness(0.f))
        .FlexShrink(0.f).Background(0xFFFAFAFA));   // 上下左右内边距全 0：高度 = 按钮高度，首钮贴左边

    // 工具按钮：无边框、无圆角、常态透明、紧密排列；hover 浅灰底，选中蓝底。
    auto boxButton = [toolbar]() -> dui::Panel*
    {
        auto* b = toolbar->AddChild<dui::Panel>(dui::Style()
            .Width(32.f).Height(30.f)
            .FlexDirection(dui::FlexDirection::Row)
            .JustifyContent(dui::Justify::Center).AlignItems(dui::Align::Center)
            .Cursor(dui::Cursor::Hand));
        b->When(dui::State::Hover, dui::Style().Background(0xFFEFF1F6));
        b->When(dui::State::Selected, dui::Style().Background(0xFFDCEAFF));
        return b;
    };
    auto iconTool = [boxButton](wchar_t glyph, auto fn) -> dui::Panel*
    {
        auto* b = boxButton();
        IconFont::CreateIcon(b, glyph, dui::Style()
            .Width(18.f).Height(18.f).FontSize(16.f).Color(0xFF6B7280));
        b->OnClick([fn](dui::PointerEvent&) { fn(); });
        return b;
    };

    // 行内格式（对当前选区切换，需先选中文字才生效）
    auto idx = [](dui::richtext::InlineStyle style) { return static_cast<size_t>(style); };
    inlineStyleBtns_[idx(dui::richtext::InlineStyle::Bold)] =
        iconTool(IconFont::Bold, [this] { if (editor_->ToggleBold()) MarkDirty(); });
    inlineStyleBtns_[idx(dui::richtext::InlineStyle::Italic)] =
        iconTool(IconFont::Italic, [this] { if (editor_->ToggleItalic()) MarkDirty(); });
    inlineStyleBtns_[idx(dui::richtext::InlineStyle::Underline)] =
        iconTool(IconFont::Underline, [this] { if (editor_->ToggleUnderline()) MarkDirty(); });
    inlineStyleBtns_[idx(dui::richtext::InlineStyle::Strike)] =
        iconTool(IconFont::Strike, [this] { if (editor_->ToggleStrike()) MarkDirty(); });
    // 块级：分割线 / 无序列表 / 引用 / 图片
    iconTool(IconFont::HorizonLine, [this] { AppendBlock(rt::BlockKind::Rule); });
    iconTool(IconFont::ListBullet, [this] { if (editor_->MakeList()) MarkDirty(); });
    iconTool(IconFont::Quote, [this] { AppendBlock(rt::BlockKind::Quote); });
    iconTool(IconFont::Image, [this] { InsertImageFile(); });

    scroller_ = panel_->AddChild<dui::Scroller>(dui::Style().FlexGrow(1.f));
    editor_ = scroller_->AddChild<dui::RichEditor>(dui::Style()
        .FlexGrow(1.f).Padding(dui::Thickness(8.f)).Background(0xFFFFFFFF)
        .LineHeight(1.1f));
    editor_->OnKeyDown([this](dui::KeyEvent&) { MarkDirty(); });
    editor_->OnChar([this](dui::KeyEvent&) { MarkDirty(); });
    editor_->OnPointerUp([this](dui::PointerEvent&) { MarkDirty(); });
    editor_->OnDropFiles([this](dui::DropFilesEvent&) { MarkDirty(); });

    // 编辑光标 / 选区所在 run 的样式：对应格式按钮选中。
    editor_->OnActiveStylesChanged([this](const dui::richtext::InlineStyleState& s)
    {
        UpdateInlineStyleButtons(s);
    });

    ApplyLayout();
}

void Detail::Show(Kind kind, int64_t detailId, int64_t entityId, const dui::String& title)
{
    CommitTitle();   // 切换实体前先提交上一个标题，避免写到新实体上
    Flush();
    kind_ = kind;
    detailId_ = detailId;
    entityId_ = entityId;
    if (kind != Kind::Line)
    {
        dirLineId_ = 0;
        dirBar_->SetStyle(dui::Style().Display(dui::Display::None));
    }
    title_->SetText(title);
    lastTitle_ = title;
    LoadDoc(detailId);
    editor_->SetDoc(doc_);
    editor_->SetEditable(detailId != 0);
    lastSavedJson_ = RichDocStore::Save(doc_);
    dirty_ = false;
    ApplyLayout();
    // 面板随内容显隐、占位变化 → 外层三栏要重排。
    if (onResize_) onResize_();
}

void Detail::ShowDirection(int64_t lineId, int direction)
{
    if (kind_ != Kind::Line)
    {
        dirBar_->SetStyle(dui::Style().Display(dui::Display::None));
        dirLineId_ = 0;
        return;
    }

    dirLineId_ = lineId;
    direction_ = (direction >= 0 && direction <= 2) ? direction : 0;
    SetDirection(direction_, false);   // 仅刷新单选图标，不写库
    dirBar_->SetStyle(dui::Style()
        .Height(38.f).FlexShrink(0.f)
        .FlexDirection(dui::FlexDirection::Row).AlignItems(dui::Align::Center)
        .Padding(dui::Thickness(12.f, 0.f, 8.f, 0.f)).Gap(16.f)
        .Background(0xFFFFFFFF)
        .Display(dui::Display::Flex));
}

void Detail::SetDirection(int direction, bool notify)
{
    direction_ = direction;
    for (int i = 0; i < 3; ++i)
        dirRadios_[i]->SetIcon(i == direction ? dui::IconName::RadioChecked
                                              : dui::IconName::Radio);
    if (notify && dirLineId_ != 0 && onLineDirection_) onLineDirection_(dirLineId_, direction);
}

void Detail::Clear()
{
    if (kind_ == Kind::None) return;   // 已经是空状态，别重复清
    CommitTitle();
    Flush();
    kind_ = Kind::None;
    detailId_ = 0;
    entityId_ = 0;
    dirLineId_ = 0;
    title_->SetText(L"");
    lastTitle_.clear();
    dirBar_->SetStyle(dui::Style().Display(dui::Display::None));
    RichDocStore::Load(std::string(), doc_);   // 编辑器内容一并清空
    editor_->SetDoc(doc_);
    lastSavedJson_ = RichDocStore::Save(doc_);
    dirty_ = false;
    ApplyLayout();
    if (onResize_) onResize_();
}

void Detail::CommitTitle()
{
    if (entityId_ == 0 || (kind_ != Kind::Node && kind_ != Kind::Line)) return;
    dui::String text = title_->GetText();
    if (text == lastTitle_) return;
    if (kind_ == Kind::Node && text.empty()) text = L"未命名";
    lastTitle_ = text;
    title_->SetText(text);
    if (onTitleCommit_) onTitleCommit_(kind_, entityId_, text);
}

void Detail::SetTitle(const dui::String& title)
{
    title_->SetText(title);
    lastTitle_ = title;
}

void Detail::UpdateTitleIf(Kind kind, int64_t detailId, const dui::String& title)
{
    if (kind_ == kind && detailId_ == detailId)
    {
        title_->SetText(title);
        lastTitle_ = title;
    }
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

void Detail::UpdateInlineStyleButtons(const dui::richtext::InlineStyleState& state)
{
    using dui::richtext::InlineStyle;
    using dui::richtext::StyleState;
    auto set = [this](InlineStyle style, StyleState value)
    {
        dui::Panel* btn = inlineStyleBtns_[static_cast<size_t>(style)];
        if (btn != nullptr)
            btn->SetState(dui::State::Selected, value == StyleState::On);
    };
    set(InlineStyle::Bold, state.bold);
    set(InlineStyle::Italic, state.italic);
    set(InlineStyle::Underline, state.underline);
    set(InlineStyle::Strike, state.strike);
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

void Detail::SetWidth(float width)
{
    width_ = width;
    ApplyLayout();
}

void Detail::ApplyLayout()
{
    // 面板随内容显隐：选中节点/连线时展开，否则收起且不占位。
    const bool visible = HasContent();
    splitter_->Set(dui::Prop::Width, visible ? kSplitterWidth : 0.f);
    splitter_->Set(dui::Prop::Visible, visible ? 1u : 0u);
    panel_->Set(dui::Prop::Width, visible ? width_ : 0.f);
    panel_->Set(dui::Prop::Visible, visible ? 1u : 0u);
}
