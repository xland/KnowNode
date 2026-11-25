#pragma once

#include <functional>
#include <memory>
#include <vector>

#include "Canvas.h"
#include "DUI/DUI.h"
#include "KnowList.h"
#include "Detail.h"
#include "KnowNew.h"
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
    void CreateNewKnowledgeButton();
    void ShowNewKnowledgeWindow();
    void BuildDialogOverlay(dui::Panel* root);
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
    dui::Panel* blank_ = nullptr;            // 未选中知识时的空白页
    dui::Button* newKnowledgeBtn_ = nullptr; // 仅在空白页存在
    std::unique_ptr<KnowNew> newKnowledge_;  // 新建知识窗口，关闭后延迟释放
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
    int64_t selectedLineDetailId_ = 0;   // 当前选中连线的详情 id（0 = 未选中连线）
};
