#include "KnWindow.h"

#include <algorithm>
#include <cstdlib>

#include "Db/KnowLine.h"
#include "Db/KnowList.h"
#include "Db/KnowNode.h"
#include "Db/Setting.h"
#include "ImageAssetStore.h"

namespace
{
    constexpr float kListMin = 220.f;
    constexpr float kDetailMin = 300.f;
    constexpr float kCanvasMin = 300.f;      // 详情按设定宽度占位，画布是被挤压的一方
    constexpr float kSplitterWidth = 4.f;    // splitter 命中区宽度（同老项目 .splitter）

    float LoadWidth(const char* key, float fallback, float minWidth)
    {
        const std::string v = db::Setting::instance().get(key);
        if (v.empty()) return fallback;
        const float w = std::strtof(v.c_str(), nullptr);
        return w >= minWidth ? w : fallback;
    }

}

void KnWindow::OnCreate()
{
    listWidth_ = LoadWidth("know_list.width", kListMin, kListMin);
    BuildUi();
    detail_->SetWidth(LoadWidth("know_detail.width", 600.f, kDetailMin));

    const auto lists = db::KnowList::instance().all();
    OpenList(lists.empty() ? 0 : lists.front().id);   // 无知识 → 空白页；有知识 → 选第一个
    category_->Refresh();
    ApplyPanelWidths();
}

bool KnWindow::OnClosing()
{
    detail_->Flush();
    if (newKnowledge_)
    {
        newKnowledge_->SetDone(nullptr);   // 断开业务回调，避免持有本窗口的悬空 this
        newKnowledge_->Close();            // closed 回调会在下一轮 PostUi 里释放对象
    }
    return true;
}

void KnWindow::OnStateChanged()
{
    titleBar_->SyncMaximizeState();
}

void KnWindow::OnDeactivated()
{
    canvas_->ClearGraphSelection();   // 切到别的应用/窗口后取消节点选中
}

void KnWindow::ApplyPanelWidths()
{
    // avail = 列表 + 画布 + 详情（splitter 已从窗口宽度中扣除）。
    const float bodyWidth = ClientSizeDip().x;
    const bool hasDetail = detail_->HasContent();
    const float fixed = kSplitterWidth + (hasDetail ? kSplitterWidth : 0.f);
    const float avail = bodyWidth - fixed;
    const float detailNow = hasDetail ? detail_->Width() : 0.f;

    // 优先保列表与详情最小值；画布/空白页不足时按老项目规则牺牲它。
    listWidth_ = std::clamp(listWidth_, kListMin,
        std::max(kListMin, avail - kCanvasMin - detailNow));
    if (hasDetail)
    {
        detail_->SetWidth(std::clamp(detail_->Width(), kDetailMin,
            std::max(kDetailMin, avail - listWidth_ - kCanvasMin)));
    }

    category_->Set(dui::Prop::Width, listWidth_);
    Root()->InvalidateLayout();
}

void KnWindow::SavePanelWidths()
{
    db::Setting::instance().set("know_list.width", std::to_string(static_cast<int>(listWidth_ + 0.5f)));
    db::Setting::instance().set("know_detail.width", std::to_string(static_cast<int>(detail_->Width() + 0.5f)));
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

    category_ = body->AddChild<KnowList>(dui::Style().Width(listWidth_));
    category_->OnSelect([this](int64_t id) { OpenList(id); });
    category_->OnAddRequest([this] { ShowNewKnowledgeWindow(); });
    category_->OnRenameRequest([this](int64_t id) { ShowRenameKnowledgeWindow(id); });
    category_->SetMenuPopup([this](const dui::Menu& menu, dui::Vec2 pos)
    {
        ShowMenu(menu, pos);
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
        if (selectedNodeDetailId_ != 0 && detailId == selectedNodeDetailId_)
            dui::Emit(kStatusSelectionEvent,
                      StatusSelectionEvent{ StatusSelectionKind::Node, text });   // 选中的节点改名 → 左下角同步
    });
    canvas_->OnLineDetailTitle([this](int64_t detailId, const dui::String& text)
    {
        detail_->UpdateTitleIf(Detail::Kind::Line, detailId, text);
        if (selectedLineDetailId_ != 0 && detailId == selectedLineDetailId_)
            dui::Emit(kStatusSelectionEvent,
                      StatusSelectionEvent{ StatusSelectionKind::Line, text });   // 选中的连线改名 → 左下角同步
    });
    canvas_->OnDeleted([this]
    {
        detail_->Clear();      // 内部会落盘：被删对象的最后几笔编辑不丢
        OpenList(currentListId_);
    });
    canvas_->OnNodeCount([this](size_t count)
    {
        statusBar_->SetNodeCount(count);
    });
    canvas_->SetMenuPopup([this](const dui::Menu& menu, dui::Vec2 pos)
    {
        ShowMenu(menu, pos);
    });

    detail_ = body->AddChild<Detail>();
    detail_->OnLineDirection([this](int64_t lineId, int direction)
    {
        canvas_->SetLineDirection(lineId, direction);
    });
    detail_->OnTitleCommit([this](Detail::Kind kind, int64_t entityId, const dui::String& text)
    {
        if (kind == Detail::Kind::Node) canvas_->RenameNodeById(entityId, text);
        else if (kind == Detail::Kind::Line) canvas_->RenameLineById(entityId, text);
    });
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
    // 所有图片（选择 / 粘贴 / 拖放）统一落到数据目录 imgs，文档只保存 file:// URI。
    detail_->SetImageSrcMapper([](const dui::String& src)
    {
        return ImageAssetStore::Save(src);
    });

    // 未选中任何知识时的空白页：占满画布+详情区域。按钮仅在空白页时动态创建。
    blank_ = body->AddChild<dui::Panel>(dui::Style()
        .FlexGrow(1.f).FlexDirection(dui::FlexDirection::Column)
        .JustifyContent(dui::Justify::Center).AlignItems(dui::Align::Center)
        .Background(0xFFF7F8FA));

    statusBar_ = Root()->AddChild<StatusBar>();
}

void KnWindow::CreateNewKnowledgeButton()
{
    if (newKnowledgeBtn_ != nullptr) return;
    newKnowledgeBtn_ = blank_->AddChild<dui::Button>(L"新建知识", dui::Style()
        .Padding(dui::Thickness(12.f, 26.f))
        .FontSize(15.f).Color(0xFFFFFFFF).Background(0xFF1677FF)
        .CornerRadius(6.f).Cursor(dui::Cursor::Hand));
    newKnowledgeBtn_->When(dui::State::Hover, dui::Style().Background(0xFF4096FF));
    newKnowledgeBtn_->OnClick([this](dui::PointerEvent&) { ShowNewKnowledgeWindow(); });
}

void KnWindow::ShowNewKnowledgeWindow()
{
    ShowKnowledgeWindow(0, L"");
}

void KnWindow::ShowRenameKnowledgeWindow(int64_t listId)
{
    db::KnowListItem item;
    if (!db::KnowList::instance().get(listId, item)) return;
    ShowKnowledgeWindow(listId, dui::Utf8ToWide(item.name.c_str()));
}

void KnWindow::ShowKnowledgeWindow(int64_t listId, const dui::String& initial)
{
    if (newKnowledge_ != nullptr) return;   // 已打开，不重复创建
    dui::Lifetime life = category_->Lifetime();
    newKnowledge_ = std::make_unique<KnowNew>([this, life](int64_t id)
    {
        if (id != 0) OpenList(id);
    }, listId, initial);
    newKnowledge_->SetClosed([life, this]
    {
        dui::PostUi(life, [this] { newKnowledge_.reset(); });   // 脱离 OnClosing 栈后释放
    });
    App()->AddWindow(*newKnowledge_);
}

void KnWindow::OpenList(int64_t listId)
{
    const bool hasKnowledge = listId != 0;
    currentListId_ = listId;

    // 画布/详情与空白页互斥：用 Display 而非 Visible，空态下不占任何布局空间。
    canvas_->Set(dui::Prop::Display, static_cast<uint32_t>(hasKnowledge ? dui::Display::Flex : dui::Display::None));
    detail_->Set(dui::Prop::Display, static_cast<uint32_t>(hasKnowledge ? dui::Display::Flex : dui::Display::None));
    blank_->Set(dui::Prop::Display, static_cast<uint32_t>(hasKnowledge ? dui::Display::None : dui::Display::Flex));

    // 按钮只在空白页存在：进入知识时销毁，进入空白页时创建。
    if (hasKnowledge)
    {
        if (newKnowledgeBtn_ != nullptr)
        {
            blank_->RemoveChild(newKnowledgeBtn_);
            newKnowledgeBtn_ = nullptr;
        }
    }
    else
        CreateNewKnowledgeButton();

    selectedNodeDetailId_ = 0;
    selectedLineDetailId_ = 0;
    dui::Emit(kStatusSelectionEvent, StatusSelectionEvent{});   // 切换知识 / 进入空白页：清空选中态

    if (!hasKnowledge)
    {
        // 进入空白页：清掉当前详情，避免残留旧知识内容。
        detail_->Clear();
        category_->SetSelected(0);
        statusBar_->SetNodeCount(0);
        ApplyPanelWidths();
        return;
    }

    category_->SetSelected(listId);
    canvas_->SetList(listId);

    // 知识本身没有详情：切换知识后详情面板回到「无内容」状态（收起、不占宽度）。
    // Clear 内部会落盘并提交标题，上一份内容的最后几笔编辑不会丢。
    detail_->Clear();
    ApplyPanelWidths();
}

void KnWindow::SelectGraphItem(int64_t nodeId, int64_t lineId)
{
    if (nodeId != 0)
    {
        selectedLineDetailId_ = 0;
        db::KnowNodeItem item;
        if (db::KnowNode::instance().get(nodeId, item))
        {
            const dui::String title = dui::Utf8ToWide(item.title.c_str());
            detail_->Show(Detail::Kind::Node, item.detailId, nodeId, title);
            selectedNodeDetailId_ = item.detailId;
            dui::Emit(kStatusSelectionEvent,
                      StatusSelectionEvent{ StatusSelectionKind::Node, title });
        }
        return;
    }
    if (lineId != 0)
    {
        selectedLineDetailId_ = 0;
        db::KnowLineItem item;
        dui::String title;
        if (db::KnowLine::instance().get(lineId, item))
        {
            // 连线标题为空时回落「连线」，与中点标签「空则不显示」的语义区分开：
            // 详情面板总得有个标题。
            title = dui::Utf8ToWide(item.title.c_str());
            detail_->Show(Detail::Kind::Line, item.detailId, lineId,
                          title.empty() ? dui::String(L"连线") : title);

            detail_->ShowDirection(lineId, item.direction);
            selectedLineDetailId_ = item.detailId;
        }
        selectedNodeDetailId_ = 0;
        dui::Emit(kStatusSelectionEvent,
                  StatusSelectionEvent{ StatusSelectionKind::Line, title.empty() ? L"连线" : title });
        return;
    }

    // 点画布空白：没有选中任何对象 → 详情面板收起。
    detail_->Clear();
    selectedNodeDetailId_ = 0;
    selectedLineDetailId_ = 0;
    dui::Emit(kStatusSelectionEvent, StatusSelectionEvent{});   // 点空白取消选中
}

