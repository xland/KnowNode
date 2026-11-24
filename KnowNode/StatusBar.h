#pragma once

#include "DUI/DUI.h"

// 状态栏左侧选中提示的全局事件契约。任何逻辑均可触发：
//   dui::Emit(kStatusSelectionEvent, StatusSelectionEvent{ kind, title });
enum class StatusSelectionKind { None, Node, Line };

struct StatusSelectionEvent
{
    StatusSelectionKind kind  = StatusSelectionKind::None;
    dui::String         title;
};

inline const dui::String kStatusSelectionEvent = L"knownode.status.selection";

// 底部状态栏：左侧显示当前选中节点/连线的名称（未选中则提示），右侧显示节点数量。
class StatusBar final : public dui::Panel
{
public:
    StatusBar();

    void SetNodeCount(size_t count);

private:
    void SetSelectedNode(const dui::String& name);
    void SetSelectedLine(const dui::String& name);
    void ClearSelection();

    dui::Text* name_ = nullptr;
    dui::Text* count_ = nullptr;
};
