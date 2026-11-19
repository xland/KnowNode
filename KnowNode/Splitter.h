#pragma once

#include <functional>

#include "DUI/DUI.h"

// 水平分隔条：封装拖拽捕获，把水平位移（DIP）回调给持有者。
// 面板宽度的最小/最大约束由持有者决定，控件本身不耦合业务。
// 命中区即 MeasureContent 返回的宽度（6 DIP）；平时透明，hover 高亮由持有者设置。
class Splitter final : public dui::Element
{
public:
    void OnDragStart(std::function<void()> fn) { onDragStart_ = std::move(fn); }
    void OnDrag(std::function<void(float totalDx)> fn) { onDrag_ = std::move(fn); }
    void OnDragEnd(std::function<void()> fn) { onDragEnd_ = std::move(fn); }

protected:
    dui::Vec2 MeasureContent(const dui::StyleContext&, const dui::Vec2&) const override
    {
        return { 6.f, 0.f };
    }
    bool HandlePointerDown(dui::PointerEvent& ev) override;
    bool HandlePointerMove(dui::PointerEvent& ev) override;
    bool HandlePointerUp(dui::PointerEvent& ev) override;

private:
    bool dragging_ = false;
    float startX_ = 0.f;
    std::function<void()> onDragStart_;
    std::function<void(float totalDx)> onDrag_;
    std::function<void()> onDragEnd_;
};
