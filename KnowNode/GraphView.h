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

    dui::Vec2 ToLocal(const dui::Vec2& abs) const;
    dui::Vec2 ToModel(const dui::Vec2& local) const;
    dui::Vec2 ToScreen(const dui::Vec2& model) const;

    int HitNode(const dui::Vec2& local) const;
    int HitLine(const dui::Vec2& local) const;
    void RefreshLabels();

    std::vector<GraphNodeView> nodes_;
    std::vector<GraphLineView> lines_;
    std::vector<dui::Text*> labels_;

    dui::String search_;
    int64_t selectedNode_ = 0;
    int64_t selectedLine_ = 0;

    float zoom_ = 1.f;
    dui::Vec2 pan_{ 0.f, 0.f };

    bool draggingNode_ = false;
    bool panning_ = false;
    int dragIndex_ = -1;
    int64_t dragNodeId_ = 0;
    dui::Vec2 dragOffset_{ 0.f, 0.f };
    dui::Vec2 lastLocal_{ 0.f, 0.f };
    dui::Vec2 pressLocal_{ 0.f, 0.f };

    uint64_t lastClickMs_ = 0;
    int64_t lastClickNode_ = 0;

    ContextMenuFn contextMenu_;
    OpenNodeFn openNode_;
    NodeMovedFn nodeMoved_;
    SelectFn select_;
};
