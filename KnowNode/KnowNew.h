#pragma once

#include <functional>

#include "DUI/DUI.h"

// “新建知识”独立窗口：输入名称后写库，成功回调交付新建的 list id。
// 窗口对象由调用方持有（App::AddWindow 只保存裸指针），关闭后由调用方释放。
class KnowNew final : public dui::Window
{
public:
    using DoneFn = std::function<void(int64_t listId)>;   // 0 = 取消
    using ClosedFn = std::function<void()>;

    explicit KnowNew(DoneFn done);

    void SetDone(DoneFn fn) { done_ = std::move(fn); }
    void SetClosed(ClosedFn fn) { closed_ = std::move(fn); }

protected:
    void OnCreate() override;
    bool OnClosing() override;

private:
    void Confirm();

    dui::TextBox* input_ = nullptr;
    int64_t resultId_ = 0;
    DoneFn done_;
    ClosedFn closed_;
};
