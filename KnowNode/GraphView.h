#pragma once

#include <functional>
#include <vector>

#include "DUI/DUI.h"

struct GraphNodeView
{
    int64_t id = 0;
    dui::String title;
    double x = 0.0;
    double y = 0.0;
    int color = 0;
};

struct GraphLineView
{
    int64_t id = 0;
    int64_t a = 0;
    int64_t b = 0;
    int color = 0;
};

class GraphView final : public dui::Panel
{
public:
    GraphView();

    void SetData(std::vector<GraphNodeView> nodes, std::vector<GraphLineView> lines);
    void RequestCenter();   // 加载/切换知识后：下一帧把所有节点居中（普通 Refresh 不触发）
    void SetSearch(const dui::String& text);
    void ResetView();

    using ContextMenuFn = std::function<void(dui::Vec2 pos, int64_t nodeId,
                                             int64_t lineId, double modelX, double modelY)>;
    void OnContextMenu(ContextMenuFn fn) { contextMenu_ = std::move(fn); }

    using OpenNodeFn = std::function<void(int64_t nodeId)>;
    void OnOpenNode(OpenNodeFn fn) { openNode_ = std::move(fn); }

    using NodeMovedFn = std::function<void(int64_t id, double x, double y)>;
    void OnNodeMoved(NodeMovedFn fn) { nodeMoved_ = std::move(fn); }

    using SelectFn = std::function<void(int64_t nodeId, int64_t lineId)>;
    void OnSelect(SelectFn fn) { select_ = std::move(fn); }

    using AddNodeFn = std::function<void(int64_t sourceNodeId)>;
    void OnAddNode(AddNodeFn fn) { addNode_ = std::move(fn); }

    // 从节点边缘拉线到另一节点松手 → 请求建立无向关联（a、b 为节点 id）。
    using LinkNodesFn = std::function<void(int64_t a, int64_t b)>;
    void OnLinkNodes(LinkNodesFn fn) { linkNodes_ = std::move(fn); }

    using CommitTitleFn = std::function<void(int64_t nodeId, const dui::String& text)>;
    void OnCommitTitle(CommitTitleFn fn) { commitTitle_ = std::move(fn); }

    // 就地编辑：在节点框上覆盖输入框，全选等待输入；编辑中的节点 id（0 = 未编辑）。
    int64_t EditingNodeId() const noexcept { return editingNodeId_; }
    void BeginTitleEdit(int64_t nodeId);
    void CommitTitleEdit(bool commit);   // commit=true 写库（空则回落"未命名"），false 放弃
    bool HandleTabKey();                 // Tab：提交当前编辑并以该节点为源新建关联节点
    void UpdateNodeTitle(int64_t nodeId, const dui::String& text);   // 仅更新标题，不重置视图

protected:
    dui::Vec2 MeasureContent(const dui::StyleContext& ctx, const dui::Vec2& avail) const override;
    void OnPaint(dui::graphics::Painter& p, const dui::StyleContext& ctx, float scale) override;
    bool HandlePointerDown(dui::PointerEvent& ev) override;
    bool HandlePointerMove(dui::PointerEvent& ev) override;
    bool HandlePointerUp(dui::PointerEvent& ev) override;
    bool HandleWheel(dui::WheelEvent& ev) override;

private:
    static float NodeWidth(const dui::String& title);
    static float NodeHeight();

    // 选中节点右侧加号圆心（相对 graph 的 local 坐标）；无可显示节点时返回 nullptr。
    const GraphNodeView* AddButtonNode() const;

    dui::Vec2 ToLocal(const dui::Vec2& abs) const;
    dui::Vec2 ToModel(const dui::Vec2& local) const;
    dui::Vec2 ToScreen(const dui::Vec2& model) const;

    int HitNode(const dui::Vec2& local) const;
    int HitLine(const dui::Vec2& local) const;
    // 节点矩形边缘 6px（屏幕恒定，按 zoom 折算回 local）命中：返回边缘节点索引，否则 -1。
    int HitNodeEdge(const dui::Vec2& local) const;
    // 拉线候选目标：非源节点、且与源尚未连过（无向）。返回节点索引，否则 -1。
    int LinkTargetAt(const dui::Vec2& local) const;
    void SetCursorIfChanged(dui::Cursor c);
    void RefreshLabels();
    void CenterView();     // 节点包围盒中心置于视口正中（按钮与加载后共用）

    std::vector<GraphNodeView> nodes_;
    std::vector<GraphLineView> lines_;
    std::vector<dui::Text*> labels_;

    dui::TextBox* editor_ = nullptr;
    int64_t editingNodeId_ = 0;

    dui::String search_;
    int64_t selectedNode_ = 0;
    int64_t selectedLine_ = 0;

    float zoom_ = 1.f;
    dui::Vec2 pan_{ 0.f, 0.f };

    bool pendingCenter_ = true;   // SetData 后延迟到 OnPaint（Bounds 有效）执行加载居中

    bool draggingNode_ = false;
    bool panning_ = false;
    bool linking_ = false;          // 正在从节点边缘拉线
    int dragIndex_ = -1;
    int64_t dragNodeId_ = 0;        // 拉线源节点 / 被拖节点 id
    int linkHoverIndex_ = -1;       // 拉线过程中指针下的目标节点（-1 = 无）
    dui::Vec2 dragOffset_{ 0.f, 0.f };
    dui::Vec2 linkPoint_{ 0.f, 0.f };  // 预览线终点（local 坐标）
    dui::Vec2 lastLocal_{ 0.f, 0.f };
    dui::Vec2 pressLocal_{ 0.f, 0.f };
    dui::Cursor cursor_ = dui::Cursor::Arrow;

    uint64_t lastClickMs_ = 0;
    int64_t lastClickNode_ = 0;

    ContextMenuFn contextMenu_;
    OpenNodeFn openNode_;
    NodeMovedFn nodeMoved_;
    SelectFn select_;
    AddNodeFn addNode_;
    LinkNodesFn linkNodes_;
    CommitTitleFn commitTitle_;
};
