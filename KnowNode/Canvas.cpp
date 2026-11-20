#include "Canvas.h"

#include <algorithm>

#include "DUI/UI/Svg.h"

#include "Db/KnowLine.h"
#include "Db/KnowNode.h"

namespace
{
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

    const wchar_t* ColorName(int color)
    {
        switch (color)
        {
        case 0: return L"无";
        case 1: return L"红";
        case 2: return L"橙";
        case 3: return L"黄";
        case 4: return L"绿";
        case 5: return L"蓝";
        case 6: return L"紫";
        default: return L"无";
        }
    }
}

Canvas::Canvas()
{
    SetStyle(dui::Style()
        .FlexGrow(1.f).Position(dui::Position::Relative)
        .FlexDirection(dui::FlexDirection::Column));

    graph_ = AddChild<GraphView>(dui::Style()
        .Position(dui::Position::Relative)
        .FlexGrow(1.f)
        .Overflow(dui::Overflow::Hidden)
        .Background(0xFFF7F8FA));
    graph_->OnContextMenu([this](dui::Vec2 pos, int64_t nodeId, int64_t lineId, double mx, double my)
    {
        ShowGraphMenu(pos, nodeId, lineId, mx, my);
    });
    graph_->OnOpenNode([this](int64_t nodeId) { graph_->BeginTitleEdit(nodeId); });
    graph_->OnNodeMoved([this](int64_t id, double x, double y)
    {
        KnowNode::instance().move(id, x, y);
    });
    graph_->OnSelect([this](int64_t nodeId, int64_t lineId)
    {
        if (onSelectItem_) onSelectItem_(nodeId, lineId);
    });
    graph_->OnAddNode([this](int64_t nodeId) { CreateRelatedNode(nodeId); });
    graph_->OnCommitTitle([this](int64_t nodeId, const dui::String& text)
    {
        KnowNodeItem item;
        if (!KnowNode::instance().get(nodeId, item)) return;
        const int64_t oldDetail = item.detailId;
        KnowNode::instance().updateTitle(nodeId, dui::WideToUtf8(text));
        graph_->UpdateNodeTitle(nodeId, text);
        if (onDetailTitle_) onDetailTitle_(oldDetail, text);
    });

    // 悬浮卡片样式（老项目 .knowNetSearch / .knowNetCenter）：白底、1px #e5e6eb、
    // 圆角 3、阴影 0 1px 4px rgba(0,0,0,.08)。两者绝对定位浮在画布之上。
    auto cardStyle = [] {
        return dui::Style()
            .Height(26.f).FlexShrink(0.f)
            .FlexDirection(dui::FlexDirection::Row).AlignItems(dui::Align::Center)
            .Background(0xFFFFFFFF).BorderWidth(1.f).BorderColor(0xFFE5E6EB)
            .CornerRadius(3.f).Shadow(4.f, 0x14000000, 0.f, 1.f);
    };

    auto* search = AddChild<dui::Panel>(cardStyle()
        .Position(dui::Position::Absolute).Left(10.f).Top(8.f).ZIndex(4)
        .Padding(dui::Thickness(10.f, 6.f, 10.f, 6.f)));
    auto* searchBox = search->AddChild<dui::TextBox>(dui::Style()
        .Width(160.f).Height(20.f).BorderWidth(0.f).Background(0)
        .FontSize(12.f).Color(0xFF1F2329));
    searchBox->OnTextChanged([this](const dui::String& text) { graph_->SetSearch(text); });
    auto* searchIcon = search->AddChild<dui::Icon>(dui::Style()
        .Width(14.f).Height(14.f).Color(0xFF6B7280).Cursor(dui::Cursor::Hand));
    searchIcon->SetIcon(dui::IconName::Search);
    searchIcon->When(dui::State::Hover, dui::Style().Color(0xFF1677FF));

    // 右上角「回到中心」：老项目 icon-center 四角取景框，用提供的 SVG 资源。
    auto* center = AddChild<dui::Panel>(cardStyle()
        .Position(dui::Position::Absolute).Right(10.f).Top(8.f).ZIndex(4)
        .Width(26.f).JustifyContent(dui::Justify::Center).AlignItems(dui::Align::Center)
        .Cursor(dui::Cursor::Hand));
    auto* centerIcon = center->AddChild<dui::Svg>(dui::Style().Width(14.f).Height(14.f));
    LoadSvgResource(centerIcon, L"ICON_CENTER");
    center->OnClick([this](dui::PointerEvent&) { graph_->ResetView(); });
}

void Canvas::SetList(int64_t listId)
{
    listId_ = listId;
    Refresh();
    graph_->RequestCenter();   // 仅切换知识时居中；普通 Refresh 不动视图
}

void Canvas::Refresh()
{
    std::vector<GraphNodeView> nodes;
    for (const auto& n : KnowNode::instance().ofList(listId_))
    {
        GraphNodeView v;
        v.id = n.id;
        v.title = dui::Utf8ToWide(n.title.c_str());
        v.x = n.x;
        v.y = n.y;
        v.color = n.color;
        nodes.push_back(std::move(v));
    }

    std::vector<GraphLineView> lines;
    for (const auto& l : KnowLine::instance().ofList(listId_))
    {
        GraphLineView v;
        v.id = l.id;
        v.a = l.nodeAId;
        v.b = l.nodeBId;
        v.color = l.color;
        lines.push_back(std::move(v));
    }

    const size_t nodeCount = nodes.size();
    graph_->SetData(std::move(nodes), std::move(lines));
    if (onNodeCount_) onNodeCount_(nodeCount);
}

void Canvas::ShowGraphMenu(dui::Vec2 pos, int64_t nodeId, int64_t lineId, double modelX, double modelY)
{
    dui::Menu menu;
    if (nodeId != 0)
    {
        menu.Add(L"重命名", [this, nodeId] { RenameNode(nodeId); });
        menu.Add(L"新建关联节点", [this, nodeId] { CreateRelatedNode(nodeId); });

        dui::Menu colors;
        for (int i = 0; i <= 6; ++i)
            colors.Add(ColorName(i), [this, nodeId, lineId, i] { SetGraphColor(nodeId, lineId, i); });
        menu.AddSubMenu(L"标记颜色", colors);
        menu.Separator();
        menu.Add(L"删除节点", [this, nodeId] { DeleteNode(nodeId); });
    }
    else if (lineId != 0)
    {
        dui::Menu colors;
        for (int i = 0; i <= 6; ++i)
            colors.Add(ColorName(i), [this, nodeId, lineId, i] { SetGraphColor(nodeId, lineId, i); });
        menu.AddSubMenu(L"标记颜色", colors);
        menu.Add(L"删除连线", [this, lineId] { DeleteLine(lineId); });
    }
    else
    {
        menu.Add(L"新建节点", [this, modelX, modelY] { CreateNode(modelX, modelY); });
    }
    menuPopup_(menu, pos);
}

void Canvas::RenameNode(int64_t nodeId)
{
    KnowNodeItem item;
    if (!KnowNode::instance().get(nodeId, item)) return;
    const int64_t oldDetail = item.detailId;
    textDialog_(L"节点标题", dui::Utf8ToWide(item.title.c_str()), [this, nodeId, oldDetail](const dui::String& text)
    {
        if (text.empty() || text.size() > 36) return;
        KnowNode::instance().updateTitle(nodeId, dui::WideToUtf8(text));
        Refresh();
        if (onDetailTitle_) onDetailTitle_(oldDetail, text);
    });
}

void Canvas::SetGraphColor(int64_t nodeId, int64_t lineId, int color)
{
    if (nodeId != 0) KnowNode::instance().setColor(nodeId, color);
    if (lineId != 0) KnowLine::instance().setColor(lineId, color);
    Refresh();
}

void Canvas::CreateNode(double x, double y)
{
    const int64_t id = KnowNode::instance().add(listId_, x, y);
    if (id == 0) return;
    Refresh();
    graph_->BeginTitleEdit(id);
}

void Canvas::CreateRelatedNode(int64_t nodeId)
{
    KnowNodeItem item;
    if (!KnowNode::instance().get(nodeId, item)) return;
    // 老项目：新节点在源节点右侧、y 对齐；源宽按标题字符数近似（同 GraphView::NodeWidth）。
    const dui::String title = dui::Utf8ToWide(item.title.c_str());
    const float srcWidth = std::max(72.f, std::min(220.f,
        static_cast<float>(title.size()) * 13.f + 24.f));
    const double x = item.x + srcWidth + 60.0;
    const int64_t id = KnowNode::instance().add(listId_, x, item.y);
    if (id == 0) return;
    KnowLine::instance().add(nodeId, id);
    Refresh();
    graph_->BeginTitleEdit(id);
}

bool Canvas::HandleTabKey()
{
    return graph_->HandleTabKey();
}

void Canvas::DeleteNode(int64_t nodeId)
{
    KnowNode::instance().remove(nodeId);
    if (onDeleted_) onDeleted_();
}

void Canvas::DeleteLine(int64_t lineId)
{
    KnowLine::instance().remove(lineId);
    if (onDeleted_) onDeleted_();
}
