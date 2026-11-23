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

    // 用 DUI 官方虚线 API 画两点虚线：实 7 / 空 5。
    void StrokeDashedLine(dui::graphics::Painter& p, dui::Vec2 a, dui::Vec2 b,
                          dui::Color color, float widthDip, float scale)
    {
        const dui::Vec2 pts[2] = { a, b };
        dui::graphics::StrokeDashedPolyline(p, pts, 2, color, widthDip,
                                            dui::Dash::Custom(7.f, 5.f), scale);
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
    OnPointerLeave([this](dui::PointerEvent&) { SetCursorIfChanged(dui::Cursor::Arrow); });

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
        || linking_ || editingNodeId_ != 0)
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

int GraphView::HitNodeEdge(const dui::Vec2& local) const
{
    // 边缘热区恒定屏幕 6px；local 已是缩放后的坐标，再按 zoom 折算回屏幕距离比较。
    const float band = 6.f * zoom_;
    for (size_t i = nodes_.size(); i-- > 0;)
    {
        const GraphNodeView& n = nodes_[i];
        const dui::Vec2 p = ToScreen({ static_cast<float>(n.x), static_cast<float>(n.y) });
        const float w = NodeWidth(n.title) * zoom_;
        const float h = NodeHeight() * zoom_;
        if (local.x < p.x || local.x > p.x + w ||
            local.y < p.y || local.y > p.y + h)
            continue;
        const float dx = std::min(local.x - p.x, p.x + w - local.x);
        const float dy = std::min(local.y - p.y, p.y + h - local.y);
        if (std::min(dx, dy) <= band)
            return static_cast<int>(i);
    }
    return -1;
}

int GraphView::LinkTargetAt(const dui::Vec2& local) const
{
    const int over = HitNode(local);
    if (over < 0 || nodes_[over].id == dragNodeId_) return -1;
    for (const GraphLineView& line : lines_)
    {
        if ((line.a == dragNodeId_ && line.b == nodes_[over].id) ||
            (line.b == dragNodeId_ && line.a == nodes_[over].id))
            return -1;   // 关联无方向：已连过则不是候选
    }
    return over;
}

void GraphView::SetCursorIfChanged(dui::Cursor c)
{
    if (cursor_ == c) return;
    cursor_ = c;
    Set(dui::Prop::Cursor, c);
}

bool GraphView::LineMidLocal(const GraphLineView& line, dui::Vec2& out) const
{
    const GraphNodeView* a = nullptr;
    const GraphNodeView* b = nullptr;
    for (const GraphNodeView& n : nodes_)
    {
        if (n.id == line.a) a = &n;
        if (n.id == line.b) b = &n;
    }
    if (a == nullptr || b == nullptr) return false;

    const dui::Vec2 pa = ToScreen({ static_cast<float>(a->x + NodeWidth(a->title) * 0.5),
                                   static_cast<float>(a->y + NodeHeight() * 0.5) });
    const dui::Vec2 pb = ToScreen({ static_cast<float>(b->x + NodeWidth(b->title) * 0.5),
                                   static_cast<float>(b->y + NodeHeight() * 0.5) });
    out = { (pa.x + pb.x) * 0.5f, (pa.y + pb.y) * 0.5f };
    return true;
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

    while (lineLabels_.size() < lines_.size())
        lineLabels_.push_back(AddChild<dui::Text>());

    for (size_t i = 0; i < lineLabels_.size(); ++i)
    {
        dui::Text* label = lineLabels_[i];
        const bool show = i < lines_.size() && !lines_[i].title.empty()
            && lines_[i].id != editingLineId_;
        dui::Vec2 mid{ 0.f, 0.f };
        if (!show || !LineMidLocal(lines_[i], mid))
        {
            label->SetText(L"");
            label->SetStyle(dui::Style().Position(dui::Position::Absolute)
                .Left(-100000.f).Top(-100000.f).Visible(false));
            continue;
        }

        const float font = std::max(10.f, std::min(20.f, 12.f * zoom_));
        const float labelW = std::max(28.f, static_cast<float>(lines_[i].title.size()) * font * 1.05f + 12.f);
        const float labelH = font * 1.6f;
        label->SetText(lines_[i].title);
        label->SetStyle(dui::Style()
            .Position(dui::Position::Absolute)
            .Left(mid.x - labelW * 0.5f).Top(mid.y - labelH * 0.5f)
            .Width(labelW).Height(labelH)
            .Background(0xFFFFFFFF).BorderWidth(1.f).BorderColor(0xFFE5E6EB).CornerRadius(3.f)
            .Padding(dui::Thickness(3.f, 0.f, 3.f, 0.f))
            .FontSize(font).Color(0xFF6B7280)
            .TextAlign(dui::TextAlign::Center)
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

void GraphView::RequestCenter()
{
    pendingCenter_ = true;
}

void GraphView::SetSearch(const dui::String& text)
{
    search_ = text;

    // 输入搜索词后，把第一个命中节点平移到画布正中心（与 CenterView 同一套平移公式）。
    if (!search_.empty())
    {
        for (const GraphNodeView& n : nodes_)
        {
            if (n.title.find(search_) == dui::String::npos) continue;
            const dui::Rect b = Bounds();
            const float cx = static_cast<float>(n.x + NodeWidth(n.title) * 0.5);
            const float cy = static_cast<float>(n.y + NodeHeight() * 0.5);
            pan_ = { b.w * 0.5f - cx * zoom_, b.h * 0.5f - cy * zoom_ };
            RefreshLabels();
            break;
        }
    }

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
    editingLineId_ = 0;   // 两种就地编辑共用一个输入框，切换时先清掉另一方
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

void GraphView::BeginLineTitleEdit(int64_t lineId)
{
    editingNodeId_ = 0;   // 两种就地编辑共用一个输入框，切换时先清掉另一方
    for (const GraphLineView& line : lines_)
    {
        if (line.id != lineId) continue;
        dui::Vec2 mid;
        if (!LineMidLocal(line, mid)) return;
        const float font = std::max(10.f, std::min(20.f, 12.f * zoom_));
        const float labelW = std::max(60.f, static_cast<float>(line.title.size()) * font * 1.05f + 16.f);
        const float labelH = font * 1.6f;
        editor_->SetStyle(dui::Style()
            .Position(dui::Position::Absolute)
            .Left(mid.x - labelW * 0.5f).Top(mid.y - labelH * 0.5f)
            .Width(labelW).Height(std::max(24.f, labelH))
            .Background(0xFFFFFFFF).BorderWidth(1.f).BorderColor(0xFF1677FF)
            .CornerRadius(3.f).TextAlign(dui::TextAlign::Center)
            .FontSize(font).Color(0xFF1F2329)
            .Visible(true).ZIndex(6));
        editor_->SetText(line.title);
        editor_->SelectAll();
        editor_->Focus();
        editingLineId_ = lineId;
        RefreshLabels();   // 隐藏正在编辑的连线标签，避免文字重叠
        InvalidatePaint();
        return;
    }
}

void GraphView::CommitTitleEdit(bool commit)
{
    if (editingNodeId_ == 0 && editingLineId_ == 0) return;
    const int64_t nodeId = editingNodeId_;
    const int64_t lineId = editingLineId_;
    dui::String text = editor_->GetText();
    editingNodeId_ = 0;
    editingLineId_ = 0;
    editor_->SetStyle(dui::Style()
        .Position(dui::Position::Absolute)
        .Left(-100000.f).Top(-100000.f).Visible(false).ZIndex(6));
    editor_->Blur();
    if (commit)
    {
        if (nodeId != 0 && commitTitle_)
        {
            // 老项目：节点空串回落「未命名」。
            if (text.empty()) text = L"未命名";
            commitTitle_(nodeId, text);
        }
        else if (lineId != 0 && commitLineTitle_)
            commitLineTitle_(lineId, text);   // 连线允许空串 = 不显示标题
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

void GraphView::UpdateLineTitle(int64_t lineId, const dui::String& text)
{
    for (GraphLineView& l : lines_)
    {
        if (l.id == lineId)
        {
            l.title = text;
            break;
        }
    }
    RefreshLabels();
    InvalidatePaint();
}

bool GraphView::HandleTabKey()
{
    if (editingNodeId_ != 0 || editingLineId_ != 0) return false;   // 编辑中的 Tab 由 editor 自己处理
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

    // 拉线预览：源节点中心 → 指针，蓝色虚线。与正式连线同层、画在节点之前，
    // 于是伸进节点的一段会被节点的不透明背景盖住，看起来从节点边缘引出。
    if (linking_ && dragIndex_ >= 0 && dragIndex_ < static_cast<int>(nodes_.size()))
    {
        const GraphNodeView& src = nodes_[dragIndex_];
        const dui::Vec2 center = ToScreen({ static_cast<float>(src.x + NodeWidth(src.title) * 0.5),
                                            static_cast<float>(src.y + NodeHeight() * 0.5) });
        StrokeDashedLine(p, { b.x + center.x, b.y + center.y },
                         { b.x + linkPoint_.x, b.y + linkPoint_.y },
                         0xFF1677FF, 2.f, scale);
    }

    for (size_t i = 0; i < nodes_.size(); ++i)
    {
        const GraphNodeView& n = nodes_[i];
        const dui::Vec2 pos = ToScreen({ static_cast<float>(n.x), static_cast<float>(n.y) });
        const dui::Rect box{ b.x + pos.x, b.y + pos.y,
                             NodeWidth(n.title) * zoom_, NodeHeight() * zoom_ };
        const float radius = 3.f * zoom_;

        p.FillRoundedRect(box, radius, NodeFill(n.color), 1.f, scale);

        const bool searchHit = !search_.empty() && n.title.find(search_) != dui::String::npos;
        const bool selected = selectedNode_ == n.id;
        const bool linkTarget = linking_ && static_cast<int>(i) == linkHoverIndex_;
        const dui::Color border = linkTarget ? 0xFF1677FF
                                : searchHit ? 0xFFF59E0B : NodeStroke(n.color, selected);
        p.StrokeRoundRect(box, radius, linkTarget ? 2.f : 1.f, border, scale);
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
        // 边缘 6px 热区 → 拉线建关联；矩形中间仍是拖动节点 / 双击编辑。
        if (HitNodeEdge(local) >= 0)
        {
            selectedNode_ = nodes_[ni].id;
            selectedLine_ = 0;
            if (select_) select_(selectedNode_, 0);
            linking_ = true;
            dragIndex_ = ni;
            dragNodeId_ = nodes_[ni].id;
            linkHoverIndex_ = -1;
            linkPoint_ = local;
            SetCursorIfChanged(dui::Cursor::Crosshair);
            SetCapture();
            InvalidatePaint();
            ev.handled = true;
            return true;
        }

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
        SetCursorIfChanged(dui::Cursor::Move);
        SetCapture();
        InvalidatePaint();
        ev.handled = true;
        return true;
    }

    const int li = HitLine(local);
    selectedNode_ = 0;
    selectedLine_ = li >= 0 ? lines_[li].id : 0;
    if (select_) select_(selectedNode_, selectedLine_);

    // 双击连线 → 就地编辑标题（与节点双击同构，节点双击走 openNode_ 回调）。
    if (li >= 0)
    {
        const uint64_t now = GetTickCount64();
        if (lastClickLine_ == lines_[li].id && now - lastClickMs_ < 350)
        {
            BeginLineTitleEdit(lines_[li].id);
            lastClickMs_ = 0;
            lastClickLine_ = 0;
            ev.handled = true;
            return true;
        }
        lastClickMs_ = now;
        lastClickLine_ = lines_[li].id;
    }
    else
        lastClickLine_ = 0;

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
    if (linking_)
    {
        linkPoint_ = local;
        linkHoverIndex_ = LinkTargetAt(local);
        InvalidatePaint();
        ev.handled = true;
        return true;
    }
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

    // 悬停反馈：节点边缘 → 十字（可拉线），节点中间 → 移动（可拖动），空白 → 箭头。
    if (HitNodeEdge(local) >= 0)
        SetCursorIfChanged(dui::Cursor::Crosshair);
    else if (HitNode(local) >= 0 || HitLine(local) >= 0)
        SetCursorIfChanged(dui::Cursor::Move);
    else
        SetCursorIfChanged(dui::Cursor::Arrow);
    return false;
}

bool GraphView::HandlePointerUp(dui::PointerEvent& ev)
{
    if (!draggingNode_ && !panning_ && !linking_) return false;

    if (linking_)
    {
        // 以松手位置重新判定：命中有效目标（非自身、未连过）→ 建无向关联；否则静默取消。
        const int target = LinkTargetAt(ToLocal(ev.pos));
        if (target >= 0 && linkNodes_)
            linkNodes_(dragNodeId_, nodes_[target].id);
    }
    else if (draggingNode_ && dragIndex_ >= 0 && dragIndex_ < static_cast<int>(nodes_.size()))
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
    linking_ = false;
    dragIndex_ = -1;
    linkHoverIndex_ = -1;
    SetCursorIfChanged(dui::Cursor::Arrow);
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
