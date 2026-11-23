#pragma once

#include <functional>
#include <vector>

#include "Canvas.h"
#include "DUI/DUI.h"
#include "KnowList.h"
#include "Detail.h"
#include "Splitter.h"
#include "StatusBar.h"
#include "TitleBar.h"

class KnWindow final : public dui::Window
{
public:
    KnWindow()
        : dui::Window(dui::WindowOptions{
              .title = L"KnowNode",
              .width = 1200.f,
              .height = 780.f,
              .frameless = true,
              .resizable = true }) {}

protected:
    void OnCreate() override;
    bool OnClosing() override;
    void OnStateChanged() override;

private:
    void BuildUi();
    void BuildDialogOverlay(dui::Panel* root);
    void ShowMenuAtClient(const dui::Menu& menu, dui::Vec2 clientPosDip);
    void ApplyPanelWidths();
    void SavePanelWidths();

    void OpenList(int64_t listId);
    void SelectGraphItem(int64_t nodeId, int64_t lineId);

    void ShowTextDialog(const dui::String& prompt, const dui::String& initial,
                        std::function<void(const dui::String&)> callback);
    void HideTextDialog();

    KnowList* category_ = nullptr;
    Canvas* canvas_ = nullptr;
    Detail* detail_ = nullptr;
    dui::Panel* blank_ = nullptr;   // 未选中知识时的空白页（含“新建知识”按钮）
    StatusBar* statusBar_ = nullptr;
    TitleBar* titleBar_ = nullptr;
    Splitter* listSplitter_ = nullptr;

    float listWidth_ = 300.f;
    float dragBase_ = 0.f;

    dui::Panel* dialogOverlay_ = nullptr;
    dui::Text* dialogPrompt_ = nullptr;
    dui::TextBox* dialogInput_ = nullptr;
    std::function<void(const dui::String&)> dialogCallback_;

    int64_t currentListId_ = 0;
    int64_t selectedNodeDetailId_ = 0;   // 当前选中节点的详情 id（0 = 未选中节点）
};
