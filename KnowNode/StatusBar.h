#pragma once

#include "DUI/DUI.h"

// 底部状态栏：左侧显示当前知识名，右侧显示节点数量。
class StatusBar final : public dui::Panel
{
public:
    StatusBar();

    void SetListName(const dui::String& name);
    void SetNodeCount(size_t count);

private:
    dui::Text* name_ = nullptr;
    dui::Text* count_ = nullptr;
};
