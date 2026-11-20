#pragma once

#include <functional>

#include "DUI/DUI.h"
#include "GraphView.h"

// 中央画布：封装工具栏（搜索/新建/复位）与关系图，以及节点/连线的增删改交互。
// 与外界的跨区域联动通过回调注入，不依赖具体窗口与详情面板。
class Canvas final : public dui::Panel
{
public:
    using TextDialogFn = std::function<void(const dui::String&, const dui::String&,
                                            std::function<void(const dui::String&)>)>;
    using MenuPopupFn = std::function<void(const dui::Menu&, dui::Vec2)>;

    Canvas();

    void SetList(int64_t listId);
    void Refresh();

    void OnSelectItem(std::function<void(int64_t, int64_t)> fn) { onSelectItem_ = std::move(fn); }
    void OnDetailTitle(std::function<void(int64_t, const dui::String&)> fn) { onDetailTitle_ = std::move(fn); }
    void OnDeleted(std::function<void()> fn) { onDeleted_ = std::move(fn); }
    void OnNodeCount(std::function<void(size_t)> fn) { onNodeCount_ = std::move(fn); }
    void SetTextDialog(TextDialogFn fn) { textDialog_ = std::move(fn); }
    void SetMenuPopup(MenuPopupFn fn) { menuPopup_ = std::move(fn); }
    bool HandleTabKey();   // 全局 Tab：以选中节点为源新建关联节点

private:
    void ShowGraphMenu(dui::Vec2 pos, int64_t nodeId, int64_t lineId, double modelX, double modelY);
    void RenameNode(int64_t nodeId);
    void SetGraphColor(int64_t nodeId, int64_t lineId, int color);
    void CreateNode(double x, double y);
    void CreateRelatedNode(int64_t nodeId);
    void DeleteNode(int64_t nodeId);
    void DeleteLine(int64_t lineId);

    int64_t listId_ = 0;
    GraphView* graph_ = nullptr;

    std::function<void(int64_t, int64_t)> onSelectItem_;
    std::function<void(int64_t, const dui::String&)> onDetailTitle_;
    std::function<void()> onDeleted_;
    std::function<void(size_t)> onNodeCount_;
    TextDialogFn textDialog_;
    MenuPopupFn menuPopup_;
};
