#pragma once

#include <functional>
#include <vector>

#include "DUI/DUI.h"

// 左侧知识列表：封装头部（新建）、列表渲染、选中态与右键菜单（重命名/删除）。
// 与外界的交互通过回调注入，避免依赖具体窗口。
class KnowList final : public dui::Panel
{
public:
    using MenuPopupFn = std::function<void(const dui::Menu&, dui::Vec2)>;

    KnowList();

    int64_t SelectedId() const { return selectedId_; }
    void SetSelected(int64_t id) { selectedId_ = id; Refresh(); }
    void Refresh();

    void OnSelect(std::function<void(int64_t)> fn) { onSelect_ = std::move(fn); }
    void OnAddRequest(std::function<void()> fn) { onAddRequest_ = std::move(fn); }
    void OnRenameRequest(std::function<void(int64_t)> fn) { onRenameRequest_ = std::move(fn); }
    void SetMenuPopup(MenuPopupFn fn) { menuPopup_ = std::move(fn); }

private:
    struct Row
    {
        dui::Panel* panel = nullptr;
        dui::Text* text = nullptr;
    };

    void Select(int64_t id);
    void Delete(int64_t id);

    dui::Panel* listBody_ = nullptr;
    std::vector<Row> rows_;
    int64_t selectedId_ = 0;

    std::function<void(int64_t)> onSelect_;
    std::function<void()> onAddRequest_;
    std::function<void(int64_t)> onRenameRequest_;
    MenuPopupFn menuPopup_;
};
