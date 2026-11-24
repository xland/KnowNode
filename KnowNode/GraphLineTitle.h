#pragma once

#include <functional>

#include "DUI/DUI.h"

// 连线中点的标题框：白底灰字、选中时边框变蓝。整体是一个绝对定位元素，
// 点击由元素自身接管，选中意图以 lineId 回调。
class GraphLineTitle final : public dui::Panel
{
public:
    GraphLineTitle();

    void Refresh(int64_t lineId, const dui::String& title, const dui::Vec2& mid,
                 float font, bool selected);
    void Hide();

    using ClickFn = std::function<void(int64_t lineId)>;
    void OnClick(ClickFn fn) { onClick_ = std::move(fn); }

private:
    dui::Text* label_ = nullptr;
    int64_t lineId_ = 0;
    ClickFn onClick_;
};
