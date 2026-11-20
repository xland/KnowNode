#include "GraphView.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
    // 老项目 MARK_COLORS：0 = 未着色，1..6 = 红橙黄绿蓝紫。
    dui::Color MarkColor(int color)
    {
        switch (color)
        {
        case 1:  return 0xFFF5222D;  // 红
        case 2:  return 0xFFFA8C16;  // 橙
        case 3:  return 0xFFFADB14;  // 黄
        case 4:  return 0xFF52C41A;  // 绿
        case 5:  return 0xFF1677FF;  // 蓝
        case 6:  return 0xFF722ED1;  // 紫
        default: return 0;
        }
    }

    constexpr uint8_t MixChannel(uint8_t c, uint8_t to, float ratio)
    {
        return static_cast<uint8_t>(c + (to - c) * ratio + 0.5f);
    }

    // 标记色往白里掺 75%（NODE_FILL_LIGHTEN），未着色返回白。
    dui::Color NodeFill(int color)
    {
        const dui::Color mark = MarkColor(color);
        if (mark == 0) return 0xFFFFFFFF;
        constexpr float kLighten = 0.75f;
        return 0xFF000000u
            | (MixChannel((mark >> 16) & 0xFF, 255, kLighten) << 16)
            | (MixChannel((mark >> 8) & 0xFF, 255, kLighten) << 8)
            | MixChannel(mark & 0xFF, 255, kLighten);
    }

    // 节点边框：未着色 #d9dde3，着色用标记色，选中用蓝。
    dui::Color NodeStroke(int color, bool selected)
    {
        if (selected) return 0xFF1677FF;
        const dui::Color mark = MarkColor(color);
        return mark != 0 ? mark : 0xFFD9DDE3;
    }

    // 连线：未着色 #b6bac1，着色用标记色，选中时把标记色压暗 25%（蓝色未着色选中为蓝）。
    dui::Color LineStroke(int color, bool selected)
    {
        const dui::Color mark = MarkColor(color);
        if (mark == 0) return selected ? 0xFF1677FF : 0xFFB6BAC1;
        if (!selected) return mark;
        constexpr float kDarken = 0.75f;
        return 0xFF000000u
            | (static_cast<uint8_t>(((mark >> 16) & 0xFF) * kDarken + 0.5f) << 16)
            | (static_cast<uint8_t>(((mark >> 8) & 0xFF) * kDarken + 0.5f) << 8)
            | static_cast<uint8_t>((mark & 0xFF) * kDarken + 0.5f);
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
    SetFocusable(true);
    OnKeyDown([this](dui::KeyEvent& ev)
    {
        if (ev.key == VK_TAB && HandleTabKey()) ev.handled = true;
    });

    editor_ = AddChild<dui::TextBox>(dui::Style()
        .Position(dui::Position::Absolute)
        .Left(-100000.f).Top(-100000.f)
        .Background(0xFFFFFFFF).BorderWidth(1.f).BorderColor(0xFF1677FF)
        .CornerRadius(3.f).TextAlign(dui::TextAlign::Center)
        .Visible(false).ZIndex(6));
    editor_->OnSubmit([this] { CommitTitleEdit(true); });
    editor_->OnBlur([this] { CommitTitleEdit(true); });   // 失焦即提交并隐藏输入框
    editor_->OnKeyDown([this](dui::KeyEvent& ev)
    {
        if (ev.key == VK_ESCAPE)
        {
            CommitTitleEdit(false);
            ev.handled = true;
        }
        else if (ev.key == VK_TAB)
        {
            const int64_t src = editingNodeId_;
            CommitTitleEdit(true);
            if (src != 0 && addNode_) addNode_(src);
            ev.handled = true;
        }
    });
}

dui::Vec2 GraphView::MeasureContent(const dui::StyleContext& ctx, const dui::Vec2& avail) const
{
    const dui::Vec2 base = dui::Element::MeasureContent(ctx, avail);
    return { std::max(360.f, base.x), std::max(260.f, base.y) };
}

float GraphView::NodeWidth(const dui::String& title)
{
    // 老项目：文字测量宽 + 24 padding，夹在 [72, 220]。中文按 13px/字近似。
    const float w = static_cast<float>(title.size()) * 13.f + 24.f;
    return std::max(72.f, std::min(220.f, w));
}

float GraphView::NodeHeight()
{
    return 36.f;
}

const GraphNodeView* GraphView::AddButtonNode() const
{
    if (addNode_ == nullptr || selectedNode_ == 0 || selectedLine_ != 0 || draggingNode_
        || editingNodeId_ != 0)
        return nullptr;
    for (const GraphNodeView& n : nodes_)
        if (n.id == selectedNode_) return &n;
    return nullptr;
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
            .Left(p.x).Top(p.y + (h - font * 1.6f) * 0.5f)
            .Width(std::max(20.f, w)).Height(font * 1.6f)
            .FontSize(font).Color(0xFF1F2329)
            .TextAlign(dui::TextAlign::Center)
            .TextOverflow(dui::TextOverflow::Ellipsis)
            .Visible(n.id != editingNodeId_));
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

void GraphView::RequestCenter()
{
    pendingCenter_ = true;
}

void GraphView::SetSearch(const dui::String& text)
{
    search_ = text;
    InvalidatePaint();
}

void GraphView::CenterView()
{
    const dui::Rect b = Bounds();
    if (nodes_.empty())
    {
        zoom_ = 1.f;
        pan_ = { b.w * 0.5f, b.h * 0.5f };
        RefreshLabels();
        return;
    }
    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = std::numeric_limits<double>::lowest();
    for (const GraphNodeView& n : nodes_)
    {
        minX = std::min(minX, n.x);
        minY = std::min(minY, n.y);
        maxX = std::max(maxX, n.x + NodeWidth(n.title));
        maxY = std::max(maxY, n.y + NodeHeight());
    }
    const float cx = static_cast<float>((minX + maxX) * 0.5);
    const float cy = static_cast<float>((minY + maxY) * 0.5);
    pan_ = { b.w * 0.5f - cx * zoom_, b.h * 0.5f - cy * zoom_ };
    RefreshLabels();
}

void GraphView::ResetView()
{
    CenterView();
    InvalidatePaint();
}

void GraphView::BeginTitleEdit(int64_t nodeId)
{
    for (const GraphNodeView& n : nodes_)
    {
        if (n.id != nodeId) continue;
        const dui::Vec2 p = ToScreen({ static_cast<float>(n.x), static_cast<float>(n.y) });
        const float w = NodeWidth(n.title) * zoom_;
        const float h = NodeHeight() * zoom_;
        const float font = std::max(10.f, std::min(20.f, 13.f * zoom_));
        editor_->SetStyle(dui::Style()
            .Position(dui::Position::Absolute)
            .Left(p.x).Top(p.y)
            .Width(std::max(20.f, w)).Height(std::max(20.f, h))
            .Background(0xFFFFFFFF).BorderWidth(1.f).BorderColor(0xFF1677FF)
            .CornerRadius(3.f).TextAlign(dui::TextAlign::Center)
            .FontSize(font).Color(0xFF1F2329)
            .Visible(true).ZIndex(6));
        editor_->SetText(n.title);
        editor_->SelectAll();
        editor_->Focus();
        editingNodeId_ = nodeId;
        RefreshLabels();   // 隐藏正在编辑的标签，避免文字重叠
        InvalidatePaint();
        return;
    }
}

void GraphView::CommitTitleEdit(bool commit)
{
    if (editingNodeId_ == 0) return;
    const int64_t id = editingNodeId_;
    dui::String text = editor_->GetText();
    editingNodeId_ = 0;
    editor_->SetStyle(dui::Style()
        .Position(dui::Position::Absolute)
        .Left(-100000.f).Top(-100000.f).Visible(false).ZIndex(6));
    editor_->Blur();
    if (commit && commitTitle_)
    {
        // 老项目：空串回落「未命名」。
        if (text.empty()) text = L"未命名";
        commitTitle_(id, text);
    }
    RefreshLabels();
    InvalidatePaint();
}

void GraphView::UpdateNodeTitle(int64_t nodeId, const dui::String& text)
{
    for (GraphNodeView& n : nodes_)
    {
        if (n.id == nodeId)
        {
            n.title = text;
            break;
        }
    }
    RefreshLabels();
    InvalidatePaint();
}

bool GraphView::HandleTabKey()
{
    if (editingNodeId_ != 0) return false;   // 编辑中的 Tab 由 editor 自己处理
    if (selectedNode_ != 0 && addNode_)
    {
        addNode_(selectedNode_);
        return true;
    }
    return false;
}

void GraphView::OnPaint(dui::graphics::Painter& p, const dui::StyleContext&, float scale)
{
    const dui::Rect b = Bounds();
    // 加载后延迟居中：此处 Bounds 已有效，复位缩放后把所有节点置于视口正中（与按钮一致）。
    if (pendingCenter_ && b.w > 0.f && b.h > 0.f)
    {
        pendingCenter_ = false;
        zoom_ = 1.f;
        CenterView();
    }

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
                     LineStroke(line.color, selected), 2.f, scale);
    }

    for (const GraphNodeView& n : nodes_)
    {
        const dui::Vec2 pos = ToScreen({ static_cast<float>(n.x), static_cast<float>(n.y) });
        const dui::Rect box{ b.x + pos.x, b.y + pos.y,
                             NodeWidth(n.title) * zoom_, NodeHeight() * zoom_ };
        const float radius = 3.f * zoom_;

        p.FillRoundedRect(box, radius, NodeFill(n.color), 1.f, scale);

        const bool searchHit = !search_.empty() && n.title.find(search_) != dui::String::npos;
        const bool selected = selectedNode_ == n.id;
        const dui::Color border = searchHit ? 0xFFF59E0B : NodeStroke(n.color, selected);
        p.StrokeRoundRect(box, radius, 1.f, border, scale);
    }

    // 选中节点右侧的加号（老项目 addBtn）：蓝圆 + 白十字，画在最后置于顶层。
    if (const GraphNodeView* n = AddButtonNode())
    {
        const dui::Vec2 pos = ToScreen({ static_cast<float>(n->x), static_cast<float>(n->y) });
        const float cx = b.x + pos.x + NodeWidth(n->title) * zoom_ + 15.f * zoom_;
        const float cy = b.y + pos.y + NodeHeight() * zoom_ * 0.5f;
        const float r = 9.f * zoom_;
        p.FillEllipse({ cx, cy }, r, r, 0xFF1677FF, scale);
        const float arm = 4.f * zoom_;
        const float stroke = 1.6f * zoom_;
        p.StrokeLine({ cx - arm, cy }, { cx + arm, cy }, 0xFFFFFFFF, stroke, scale);
        p.StrokeLine({ cx, cy - arm }, { cx, cy + arm }, 0xFFFFFFFF, stroke, scale);
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

    // 加号命中优先于节点：点在圆内则新建关联节点，不当作拖拽节点。
    if (const GraphNodeView* n = AddButtonNode())
    {
        const dui::Vec2 pos = ToScreen({ static_cast<float>(n->x), static_cast<float>(n->y) });
        const float cx = pos.x + NodeWidth(n->title) * zoom_ + 15.f * zoom_;
        const float cy = pos.y + NodeHeight() * zoom_ * 0.5f;
        const float dx = local.x - cx;
        const float dy = local.y - cy;
        const float r = 11.f * zoom_;   // 半径 9 + 容差 2
        if (dx * dx + dy * dy <= r * r)
        {
            addNode_(n->id);
            ev.handled = true;
            return true;
        }
    }

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
        Focus();   // 选中节点后取得焦点，Tab 才能新建关联节点
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
    InvalidatePaint();   // 拖拽结束后让加号重新出现
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
