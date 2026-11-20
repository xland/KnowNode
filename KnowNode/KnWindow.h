#pragma once

#include <functional>
#include <vector>

#include "Canvas.h"
#include "Category.h"
#include "DUI/DUI.h"
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

    Category* category_ = nullptr;
    Canvas* canvas_ = nullptr;
    Detail* detail_ = nullptr;
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
};
