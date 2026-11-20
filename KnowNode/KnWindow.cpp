#include "KnWindow.h"

#include <algorithm>
#include <cstdlib>

#include "Db/KnowLine.h"
#include "Db/KnowList.h"
#include "Db/KnowNode.h"
#include "Db/Setting.h"

namespace
{
    constexpr float kListMin = 300.f;
    constexpr float kDetailMin = 300.f;
    constexpr float kCanvasMin = 500.f;
    constexpr float kSplitterWidth = 4.f;    // splitter 命中区宽度（同老项目 .splitter）
    constexpr float kHandleWidth = 14.f;

    float LoadWidth(const char* key, float fallback, float minWidth)
    {
        const std::string v = Setting::instance().get(key);
        if (v.empty()) return fallback;
        const float w = std::strtof(v.c_str(), nullptr);
        return w >= minWidth ? w : fallback;
    }

    dui::String ListName(int64_t id)
    {
        KnowListItem item;
        return KnowList::instance().get(id, item) ? dui::Utf8ToWide(item.name.c_str()) : dui::String();
    }
}

void KnWindow::OnCreate()
{
    listWidth_ = LoadWidth("know_list.width", kListMin, kListMin);
    BuildUi();
    detail_->SetWidth(LoadWidth("know_detail.width", 600.f, kDetailMin));

    const auto lists = KnowList::instance().all();
    if (!lists.empty()) OpenList(lists.front().id);
    category_->Refresh();
    ApplyPanelWidths();
}

bool KnWindow::OnClosing()
{
    detail_->Flush();
    return true;
}

void KnWindow::OnStateChanged()
{
    titleBar_->SyncMaximizeState();
}

void KnWindow::ApplyPanelWidths()
{
    // avail = 列表 + 画布 + 详情（splitter 与手柄已从窗口宽度中扣除）。
    const float bodyWidth = ClientSizeDip().x;
    const bool expanded = detail_->IsExpanded();
    const float fixed = kSplitterWidth + kHandleWidth
        + (expanded ? kSplitterWidth : 0.f);
    const float avail = bodyWidth - fixed;
    const float detailNow = expanded ? detail_->Width() : 0.f;

    // 优先保列表与详情最小值；画布不足时按老项目规则牺牲画布。
    listWidth_ = std::clamp(listWidth_, kListMin,
        std::max(kListMin, avail - kCanvasMin - detailNow));
    if (expanded)
    {
        detail_->SetWidth(std::clamp(detail_->Width(), kDetailMin,
            std::max(kDetailMin, avail - listWidth_ - kCanvasMin)));
    }

    category_->Set(dui::Prop::Width, listWidth_);
    Root()->InvalidateLayout();
}

void KnWindow::SavePanelWidths()
{
    Setting::instance().set("know_list.width", std::to_string(static_cast<int>(listWidth_ + 0.5f)));
    Setting::instance().set("know_detail.width", std::to_string(static_cast<int>(detail_->Width() + 0.5f)));
}

void KnWindow::ShowMenuAtClient(const dui::Menu& menu, dui::Vec2 clientPosDip)
{
    POINT pt{
        static_cast<LONG>(clientPosDip.x * Scale() + 0.5f),
        static_cast<LONG>(clientPosDip.y * Scale() + 0.5f)
    };
    ClientToScreen(NativeHandle(), &pt);
    dui::ShowPopupMenu(menu, NativeHandle(),
                       dui::Vec2{ static_cast<float>(pt.x), static_cast<float>(pt.y) });
}

void KnWindow::BuildUi()
{
    Root()->SetStyle(dui::Style()
        .FlexDirection(dui::FlexDirection::Column)
        .Background(0xFFFFFFFF));

    titleBar_ = Root()->AddChild<TitleBar>();
    titleBar_->Init(this);

    auto* body = Root()->AddChild<dui::Panel>(dui::Style()
        .FlexGrow(1.f).FlexDirection(dui::FlexDirection::Row));

    category_ = body->AddChild<Category>(dui::Style().Width(listWidth_));
    category_->OnSelect([this](int64_t id) { OpenList(id); });
    category_->OnRenamed([this](int64_t id, const dui::String& name)
    {
        statusBar_->SetListName(name);
        if (detail_->CurrentKind() == Detail::Kind::List && currentListId_ == id)
            detail_->SetTitle(name);
    });
    category_->SetTextDialog([this](const dui::String& prompt, const dui::String& initial,
                                    std::function<void(const dui::String&)> callback)
    {
        ShowTextDialog(prompt, initial, std::move(callback));
    });
    category_->SetMenuPopup([this](const dui::Menu& menu, dui::Vec2 pos)
    {
        ShowMenuAtClient(menu, pos);
    });

    auto splitterStyle = [] {
        return dui::Style()
            .Width(kSplitterWidth).FlexShrink(0.f)
            .Background(0xFFEEEEEE)
            .Cursor(dui::Cursor::SizeWE);
    };
    // 列表在最左：右移 splitter → 列表变宽(+)；详情在最右：右移 → 详情变窄(−)。
    auto bindSplitter = [this](Splitter* splitter, float* width, float sign)
    {
        splitter->OnDragStart([this, width] { dragBase_ = *width; });
        splitter->OnDrag([this, width, sign](float dx)
        {
            *width = dragBase_ + sign * dx;
            ApplyPanelWidths();
        });
        splitter->OnDragEnd([this] { SavePanelWidths(); });
    };

    listSplitter_ = body->AddChild<Splitter>(splitterStyle());
    listSplitter_->When(dui::State::Hover, dui::Style().Background(0xFFdddddd));
    bindSplitter(listSplitter_, &listWidth_, 1.f);

    canvas_ = body->AddChild<Canvas>();
    canvas_->OnSelectItem([this](int64_t nodeId, int64_t lineId)
    {
        SelectGraphItem(nodeId, lineId);
    });
    canvas_->OnDetailTitle([this](int64_t detailId, const dui::String& text)
    {
        detail_->UpdateTitleIf(Detail::Kind::Node, detailId, text);
    });
    canvas_->OnDeleted([this]
    {
        detail_->Flush();
        detail_->Clear();
        OpenList(currentListId_);
    });
    canvas_->OnNodeCount([this](size_t count)
    {
        statusBar_->SetNodeCount(count);
    });
    canvas_->SetTextDialog([this](const dui::String& prompt, const dui::String& initial,
                                  std::function<void(const dui::String&)> callback)
    {
        ShowTextDialog(prompt, initial, std::move(callback));
    });
    canvas_->SetMenuPopup([this](const dui::Menu& menu, dui::Vec2 pos)
    {
        ShowMenuAtClient(menu, pos);
    });

    detail_ = body->AddChild<Detail>();
    detail_->OnToggle([this] { ApplyPanelWidths(); });
    detail_->OnResize([this] { ApplyPanelWidths(); });
    detail_->OnWidthCommitted([this] { SavePanelWidths(); });
    detail_->SetPickImage([this](dui::String& path)
    {
        static const dui::FileTypeFilter filters[] = {
            { L"图片", L"*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.tif;*.tiff;*.ico" }
        };
        dui::OpenFileOptions options;
        options.title = L"选择图片";
        options.filters = filters;
        const auto result = OpenFile(options);
        if (!result.IsOk() || result.Value().empty()) return false;
        path = result.Value().front();
        return true;
    });

    statusBar_ = Root()->AddChild<StatusBar>();

    BuildDialogOverlay(Root());
}

void KnWindow::BuildDialogOverlay(dui::Panel* root)
{
    dialogOverlay_ = root->AddChild<dui::Panel>(dui::Style()
        .Position(dui::Position::Absolute)
        .Left(0.f).Top(0.f).Right(0.f).Bottom(0.f).ZIndex(100)
        .FlexDirection(dui::FlexDirection::Column)
        .JustifyContent(dui::Justify::Center).AlignItems(dui::Align::Center)
        .Background(0x88000000).AppRegion(dui::AppRegion::NoDrag));
    dialogOverlay_->OnPointerDown([](dui::PointerEvent& e) { e.handled = true; });

    auto* card = dialogOverlay_->AddChild<dui::Panel>(dui::Style()
        .Width(340.f).Height(156.f).Padding(16.f)
        .FlexDirection(dui::FlexDirection::Column).Gap(10.f)
        .Background(0xFFFFFFFF).CornerRadius(8.f)
        .AppRegion(dui::AppRegion::NoDrag));

    dialogPrompt_ = card->AddChild<dui::Text>(L"", dui::Style()
        .FontSize(14.f).Color(0xFF111827));

    dialogInput_ = card->AddChild<dui::TextBox>(dui::Style()
        .Height(32.f).Width(306.f)
        .BorderWidth(1.f).BorderColor(0xFFD1D5DB).CornerRadius(4.f)
        .Padding(dui::Thickness(0.f, 8.f)).FontSize(14.f));
    dialogInput_->OnSubmit([this]
    {
        if (dialogCallback_) dialogCallback_(dialogInput_->GetText());
        HideTextDialog();
    });

    auto* row = card->AddChild<dui::Panel>(dui::Style()
        .FlexDirection(dui::FlexDirection::Row).JustifyContent(dui::Justify::End)
        .Gap(8.f));
    auto* cancel = row->AddChild<dui::Button>(L"取消", dui::Style()
        .Padding(dui::Thickness(6.f, 12.f)).Color(0xFF4B5563)
        .CornerRadius(4.f).Cursor(dui::Cursor::Hand));
    cancel->OnClick([this](dui::PointerEvent&) { HideTextDialog(); });

    auto* ok = row->AddChild<dui::Button>(L"确定", dui::Style()
        .Padding(dui::Thickness(6.f, 12.f)).Color(0xFFFFFFFF)
        .Background(0xFF2563EB).CornerRadius(4.f).Cursor(dui::Cursor::Hand));
    ok->OnClick([this](dui::PointerEvent&)
    {
        if (dialogCallback_) dialogCallback_(dialogInput_->GetText());
        HideTextDialog();
    });

    dialogOverlay_->Set(dui::Prop::Visible, 0u);
}

void KnWindow::OpenList(int64_t listId)
{
    if (listId == 0) return;
    currentListId_ = listId;
    detail_->SetHasKnowledge(true);
    category_->SetSelected(listId);
    canvas_->SetList(listId);

    KnowListItem item;
    if (KnowList::instance().get(listId, item))
        detail_->Show(Detail::Kind::List, item.detailId, dui::Utf8ToWide(item.name.c_str()));
    statusBar_->SetListName(ListName(listId));
}

void KnWindow::SelectGraphItem(int64_t nodeId, int64_t lineId)
{
    if (nodeId != 0)
    {
        KnowNodeItem item;
        if (KnowNode::instance().get(nodeId, item))
            detail_->Show(Detail::Kind::Node, item.detailId, dui::Utf8ToWide(item.title.c_str()));
        return;
    }
    if (lineId != 0)
    {
        KnowLineItem item;
        if (KnowLine::instance().get(lineId, item))
            detail_->Show(Detail::Kind::Line, item.detailId, L"连线");
        return;
    }

    KnowListItem item;
    if (KnowList::instance().get(currentListId_, item))
        detail_->Show(Detail::Kind::List, item.detailId, dui::Utf8ToWide(item.name.c_str()));
}

void KnWindow::ShowTextDialog(const dui::String& prompt, const dui::String& initial,
                              std::function<void(const dui::String&)> callback)
{
    dialogPrompt_->SetText(prompt);
    dialogInput_->SetText(initial);
    dialogCallback_ = std::move(callback);
    dialogOverlay_->Set(dui::Prop::Visible, 1u);
    dialogInput_->Focus();
    dialogInput_->SelectAll();
}

void KnWindow::HideTextDialog()
{
    dialogInput_->Blur();
    dialogCallback_ = nullptr;
    dialogOverlay_->Set(dui::Prop::Visible, 0u);
}


