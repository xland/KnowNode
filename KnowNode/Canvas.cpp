#include "Canvas.h"

#include "Db/KnowLine.h"
#include "Db/KnowNode.h"

namespace
{
    const wchar_t* ColorName(int color)
    {
        switch (color)
        {
        case 0: return L"无";
        case 1: return L"蓝";
        case 2: return L"绿";
        case 3: return L"橙";
        case 4: return L"红";
        case 5: return L"紫";
        case 6: return L"青";
        default: return L"无";
        }
    }
}

Canvas::Canvas()
{
    SetStyle(dui::Style()
        .FlexGrow(1.f).FlexDirection(dui::FlexDirection::Column));

    auto* head = AddChild<dui::Panel>(dui::Style()
        .Height(42.f).FlexShrink(0.f)
        .FlexDirection(dui::FlexDirection::Row).AlignItems(dui::Align::Center)
        .Gap(8.f).Padding(dui::Thickness(0.f, 10.f))
        .Background(0xFFFFFFFF));

    auto* searchBox = head->AddChild<dui::TextBox>(dui::Style()
        .Width(180.f).Height(28.f).BorderWidth(1.f).BorderColor(0xFFD1D5DB)
        .CornerRadius(4.f).Padding(dui::Thickness(0.f, 8.f)).FontSize(13.f));
    searchBox->OnTextChanged([this](const dui::String& text) { graph_->SetSearch(text); });

    auto* addNode = head->AddChild<dui::Button>(L"＋ 节点", dui::Style()
        .FontSize(13.f).Color(0xFF2563EB).Padding(dui::Thickness(4.f, 8.f))
        .Cursor(dui::Cursor::Hand));
    addNode->OnClick([this](dui::PointerEvent&) { CreateNode(80.0, 80.0); });

    auto* resetView = head->AddChild<dui::Button>(L"复位视图", dui::Style()
        .FontSize(13.f).Color(0xFF4B5563).Padding(dui::Thickness(4.f, 8.f))
        .Cursor(dui::Cursor::Hand));
    resetView->OnClick([this](dui::PointerEvent&) { graph_->ResetView(); });

    graph_ = AddChild<GraphView>(dui::Style()
        .Position(dui::Position::Relative)
        .FlexGrow(1.f)
        .Overflow(dui::Overflow::Hidden)
        .Background(0xFFF7F8FA));
    graph_->OnContextMenu([this](dui::Vec2 pos, int64_t nodeId, int64_t lineId, double mx, double my)
    {
        ShowGraphMenu(pos, nodeId, lineId, mx, my);
    });
    graph_->OnOpenNode([this](int64_t nodeId) { RenameNode(nodeId); });
    graph_->OnNodeMoved([this](int64_t id, double x, double y)
    {
        KnowNode::instance().move(id, x, y);
    });
    graph_->OnSelect([this](int64_t nodeId, int64_t lineId)
    {
        if (onSelectItem_) onSelectItem_(nodeId, lineId);
    });
}

void Canvas::SetList(int64_t listId)
{
    listId_ = listId;
    Refresh();
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
    RenameNode(id);
}

void Canvas::CreateRelatedNode(int64_t nodeId)
{
    KnowNodeItem item;
    if (!KnowNode::instance().get(nodeId, item)) return;
    const int64_t id = KnowNode::instance().add(listId_, item.x + 180.0, item.y + 90.0);
    if (id == 0) return;
    KnowLine::instance().add(nodeId, id);
    Refresh();
    RenameNode(id);
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
