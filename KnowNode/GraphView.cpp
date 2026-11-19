#include "GraphView.h"

#include <algorithm>
#include <cmath>

namespace
{
    dui::Color ColorOf(int color)
    {
        switch (color)
        {
        case 1:  return 0xFF3B82F6;
        case 2:  return 0xFF10B981;
        case 3:  return 0xFFF59E0B;
        case 4:  return 0xFFEF4444;
        case 5:  return 0xFF8B5CF6;
        case 6:  return 0xFF06B6D4;
        default: return 0xFF9CA3AF;
        }
    }

    float DistanceToSegment(const dui::Vec2& p, const dui::Vec2& a, const dui::Vec2& b)
    {
        const float dx = b.x - a.x;
        const float dy = b.y - a.y;
        const float len2 = dx * dx + dy * dy;
        if (len2 < 0.0001f)
            return std::sqrt((p.x - a.x) * (p.x - a.x) + (p.y - a.y) * (p.y - a.y));
        float t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2;
        t = std::max(0.f, std::min(1.f, t));
        const float cx = a.x + t * dx;
        const float cy = a.y + t * dy;
        return std::sqrt((p.x - cx) * (p.x - cx) + (p.y - cy) * (p.y - cy));
    }
}

GraphView::GraphView()
{
    SetStyle(dui::Style()
        .Position(dui::Position::Relative)
        .FlexGrow(1.f)
        .Overflow(dui::Overflow::Hidden)
        .Background(0xFFF7F8FA));
}

dui::Vec2 GraphView::MeasureContent(const dui::StyleContext& ctx, const dui::Vec2& avail) const
{
    const dui::Vec2 base = dui::Element::MeasureContent(ctx, avail);
    return { std::max(360.f, base.x), std::max(260.f, base.y) };
}

float GraphView::NodeWidth(const dui::String& title)
{
    const float w = 84.f + static_cast<float>(title.size()) * 13.f;
    return std::max(96.f, std::min(220.f, w));
}

float GraphView::NodeHeight()
{
    return 52.f;
}

dui::Vec2 GraphView::ToLocal(const dui::Vec2& abs) const
{
    const dui::Rect b = Bounds();
    return { abs.x - b.x, abs.y - b.y };
}

dui::Vec2 GraphView::ToModel(const dui::Vec2& local) const
{
    return { (local.x - pan_.x) / zoom_, (local.y - pan_.y) / zoom_ };
}

dui::Vec2 GraphView::ToScreen(const dui::Vec2& model) const
{
    return { pan_.x + static_cast<float>(model.x) * zoom_,
             pan_.y + static_cast<float>(model.y) * zoom_ };
}

int GraphView::HitNode(const dui::Vec2& local) const
{
    for (size_t i = nodes_.size(); i-- > 0;)
    {
        const GraphNodeView& n = nodes_[i];
        const dui::Vec2 p = ToScreen({ static_cast<float>(n.x), static_cast<float>(n.y) });
        const float w = NodeWidth(n.title) * zoom_;
        const float h = NodeHeight() * zoom_;
        if (local.x >= p.x && local.x <= p.x + w &&
            local.y >= p.y && local.y <= p.y + h)
            return static_cast<int>(i);
    }
    return -1;
}

int GraphView::HitLine(const dui::Vec2& local) const
{
    for (size_t i = lines_.size(); i-- > 0;)
    {
        const GraphLineView& line = lines_[i];
        const GraphNodeView* a = nullptr;
        const GraphNodeView* b = nullptr;
        for (const GraphNodeView& n : nodes_)
        {
            if (n.id == line.a) a = &n;
            if (n.id == line.b) b = &n;
        }
        if (a == nullptr || b == nullptr) continue;

        const dui::Vec2 pa = ToScreen({ static_cast<float>(a->x + NodeWidth(a->title) * 0.5),
                                       static_cast<float>(a->y + NodeHeight() * 0.5) });
        const dui::Vec2 pb = ToScreen({ static_cast<float>(b->x + NodeWidth(b->title) * 0.5),
                                       static_cast<float>(b->y + NodeHeight() * 0.5) });
        if (DistanceToSegment(local, pa, pb) <= 6.f)
            return static_cast<int>(i);
    }
    return -1;
}

void GraphView::RefreshLabels()
{
    while (labels_.size() < nodes_.size())
        labels_.push_back(AddChild<dui::Text>());

    for (size_t i = 0; i < labels_.size(); ++i)
    {
        dui::Text* label = labels_[i];
        if (i >= nodes_.size())
        {
            label->SetText(L"");
            label->SetStyle(dui::Style().Position(dui::Position::Absolute)
                .Left(-100000.f).Top(-100000.f).Visible(false));
            continue;
        }

        const GraphNodeView& n = nodes_[i];
        const dui::Vec2 p = ToScreen({ static_cast<float>(n.x), static_cast<float>(n.y) });
        const float w = NodeWidth(n.title) * zoom_;
        const float h = NodeHeight() * zoom_;
        const float font = std::max(10.f, std::min(20.f, 13.f * zoom_));

        label->SetText(n.title);
        label->SetStyle(dui::Style()
            .Position(dui::Position::Absolute)
            .Left(p.x + 10.f).Top(p.y + (h - font * 1.6f) * 0.5f)
            .Width(std::max(20.f, w - 20.f)).Height(font * 1.6f)
            .FontSize(font).Color(0xFF1F2933)
            .TextOverflow(dui::TextOverflow::Ellipsis)
            .Visible(true));
    }
}

void GraphView::SetData(std::vector<GraphNodeView> nodes, std::vector<GraphLineView> lines)
{
    nodes_ = std::move(nodes);
    lines_ = std::move(lines);
    selectedNode_ = 0;
    selectedLine_ = 0;
    RefreshLabels();
    InvalidatePaint();
}

void GraphView::SetSearch(const dui::String& text)
{
    search_ = text;
    InvalidatePaint();
}

void GraphView::ResetView()
{
    zoom_ = 1.f;
    pan_ = { 0.f, 0.f };
    RefreshLabels();
    InvalidatePaint();
}

void GraphView::OnPaint(dui::graphics::Painter& p, const dui::StyleContext&, float scale)
{
    const dui::Rect b = Bounds();

    for (const GraphLineView& line : lines_)
    {
        const GraphNodeView* a = nullptr;
        const GraphNodeView* c = nullptr;
        for (const GraphNodeView& n : nodes_)
        {
            if (n.id == line.a) a = &n;
            if (n.id == line.b) c = &n;
        }
        if (a == nullptr || c == nullptr) continue;

        const dui::Vec2 pa = ToScreen({ static_cast<float>(a->x + NodeWidth(a->title) * 0.5),
                                       static_cast<float>(a->y + NodeHeight() * 0.5) });
        const dui::Vec2 pc = ToScreen({ static_cast<float>(c->x + NodeWidth(c->title) * 0.5),
                                       static_cast<float>(c->y + NodeHeight() * 0.5) });
        const bool selected = selectedLine_ == line.id;
        p.StrokeLine({ b.x + pa.x, b.y + pa.y }, { b.x + pc.x, b.y + pc.y },
                     ColorOf(line.color), selected ? 3.f : 1.5f, scale);
    }

    for (const GraphNodeView& n : nodes_)
    {
        const dui::Vec2 pos = ToScreen({ static_cast<float>(n.x), static_cast<float>(n.y) });
        const dui::Rect box{ b.x + pos.x, b.y + pos.y,
                             NodeWidth(n.title) * zoom_, NodeHeight() * zoom_ };
        p.FillRoundedRect(box, 8.f * zoom_, 0xFFFFFFFF, 1.f, scale);

        const bool searchHit = !search_.empty() && n.title.find(search_) != dui::String::npos;
        const bool selected = selectedNode_ == n.id;
        const dui::Color border = searchHit ? 0xFFF59E0B : ColorOf(n.color);
        const float width = selected ? 3.f : (searchHit ? 2.5f : 1.5f);
        p.StrokeRoundRect(box, 8.f * zoom_, width, border, scale);
    }
}

bool GraphView::HandlePointerDown(dui::PointerEvent& ev)
{
    const dui::Vec2 local = ToLocal(ev.pos);
    pressLocal_ = local;

    if (ev.button == dui::PointerButton::Right)
    {
        const int ni = HitNode(local);
        const int li = ni < 0 ? HitLine(local) : -1;
        const dui::Vec2 model = ToModel(local);
        if (contextMenu_)
            contextMenu_(ev.pos,
                         ni >= 0 ? nodes_[ni].id : 0,
                         li >= 0 ? lines_[li].id : 0,
                         model.x, model.y);
        ev.handled = true;
        return true;
    }
    if (ev.button != dui::PointerButton::Left)
        return false;

    const int ni = HitNode(local);
    if (ni >= 0)
    {
        const uint64_t now = GetTickCount64();
        if (lastClickNode_ == nodes_[ni].id && now - lastClickMs_ < 350 && openNode_)
        {
            openNode_(nodes_[ni].id);
            lastClickMs_ = 0;
            lastClickNode_ = 0;
            ev.handled = true;
            return true;
        }
        lastClickMs_ = now;
        lastClickNode_ = nodes_[ni].id;

        selectedNode_ = nodes_[ni].id;
        selectedLine_ = 0;
        if (select_) select_(selectedNode_, 0);
        draggingNode_ = true;
        dragIndex_ = ni;
        dragNodeId_ = nodes_[ni].id;
        const dui::Vec2 model = ToModel(local);
        dragOffset_ = { static_cast<float>(model.x - nodes_[ni].x),
                        static_cast<float>(model.y - nodes_[ni].y) };
        SetCapture();
        InvalidatePaint();
        ev.handled = true;
        return true;
    }

    const int li = HitLine(local);
    selectedNode_ = 0;
    selectedLine_ = li >= 0 ? lines_[li].id : 0;
    if (select_) select_(selectedNode_, selectedLine_);
    panning_ = true;
    lastLocal_ = local;
    SetCapture();
    InvalidatePaint();
    ev.handled = true;
    return true;
}

bool GraphView::HandlePointerMove(dui::PointerEvent& ev)
{
    const dui::Vec2 local = ToLocal(ev.pos);
    if (draggingNode_ && dragIndex_ >= 0 && dragIndex_ < static_cast<int>(nodes_.size()))
    {
        const dui::Vec2 model = ToModel(local);
        nodes_[dragIndex_].x = static_cast<double>(model.x - dragOffset_.x);
        nodes_[dragIndex_].y = static_cast<double>(model.y - dragOffset_.y);
        RefreshLabels();
        InvalidatePaint();
        ev.handled = true;
        return true;
    }
    if (panning_)
    {
        pan_.x += local.x - lastLocal_.x;
        pan_.y += local.y - lastLocal_.y;
        lastLocal_ = local;
        RefreshLabels();
        InvalidatePaint();
        ev.handled = true;
        return true;
    }
    return false;
}

bool GraphView::HandlePointerUp(dui::PointerEvent& ev)
{
    if (!draggingNode_ && !panning_) return false;

    if (draggingNode_ && dragIndex_ >= 0 && dragIndex_ < static_cast<int>(nodes_.size()))
    {
        const dui::Vec2 local = ToLocal(ev.pos);
        if (std::abs(local.x - pressLocal_.x) + std::abs(local.y - pressLocal_.y) > 4.f &&
            nodeMoved_)
        {
            nodeMoved_(dragNodeId_, nodes_[dragIndex_].x, nodes_[dragIndex_].y);
        }
    }

    if (HasCapture()) ReleaseCapture();
    draggingNode_ = false;
    panning_ = false;
    dragIndex_ = -1;
    ev.handled = true;
    return true;
}

bool GraphView::HandleWheel(dui::WheelEvent& ev)
{
    const dui::Rect b = Bounds();
    const dui::Vec2 center{ b.w * 0.5f, b.h * 0.5f };
    const dui::Vec2 modelCenter = ToModel(center);

    const float factor = ev.delta > 0 ? 1.1f : (1.f / 1.1f);
    zoom_ = std::max(0.3f, std::min(3.f, zoom_ * factor));
    pan_.x = center.x - static_cast<float>(modelCenter.x) * zoom_;
    pan_.y = center.y - static_cast<float>(modelCenter.y) * zoom_;

    RefreshLabels();
    InvalidatePaint();
    ev.handled = true;
    return true;
}
