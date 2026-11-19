#pragma once

#include "DUI/DUI.h"

// 无边框窗口的自绘标题栏：logo、标题、最小化/最大化/关闭按钮。
// 经 Init() 注入所属窗口后，按钮点击直接调用窗口操作，业务自包含。
class TitleBar final : public dui::Panel
{
public:
    TitleBar();

    void Init(dui::Window* window);
    void SyncMaximizeState();

private:
    dui::Window* window_ = nullptr;
    dui::Icon* maxIcon_ = nullptr;
};
