#pragma once

#include <functional>

#include "DUI/DUI.h"
#include "Splitter.h"

// 右侧详情面板：封装展开手柄、可拖拽分隔条、标题/工具栏/富文本编辑器，
// 以及文档内容的加载、编辑与保存。与窗口的布局联动通过回调注入。
class Detail final : public dui::Panel
{
public:
    enum class Kind { None, List, Node, Line };

    Detail();

    void Show(Kind kind, int64_t detailId, const dui::String& title);
    void Clear();
    void Flush();

    Kind CurrentKind() const { return kind_; }
    void SetTitle(const dui::String& title);
    void UpdateTitleIf(Kind kind, int64_t detailId, const dui::String& title);

    void SetExpanded(bool expanded);
    bool IsExpanded() const { return expanded_; }
    void SetHasKnowledge(bool has);
    void SetWidth(float width);
    float Width() const { return width_; }
    void ApplyLayout();

    void OnToggle(std::function<void()> fn) { onToggle_ = std::move(fn); }
    void OnResize(std::function<void()> fn) { onResize_ = std::move(fn); }
    void OnWidthCommitted(std::function<void()> fn) { onWidthCommitted_ = std::move(fn); }
    void SetPickImage(std::function<bool(dui::String&)> fn) { pickImage_ = std::move(fn); }
    /// 连线箭头方向变更：回调参数为 (lineId, direction: 0=A→B 1=B→A 2=双向)
    void OnLineDirection(std::function<void(int64_t, int)> fn) { onLineDirection_ = std::move(fn); }
    /// 选中连线时设定方向栏上下文；kind 非 Line 时方向栏隐藏。
    void ShowDirection(int64_t lineId, int direction,
                       const dui::String& titleA, const dui::String& titleB);

private:
    void SetDirection(int direction, bool notify = true);
    void Toggle();
    void AppendBlock(dui::richtext::BlockKind kind);
    void InsertImageFile();
    void MarkDirty();
    void LoadDoc(int64_t detailId);

    dui::Panel* handle_ = nullptr;
    dui::Text* arrow_ = nullptr;   // 字体图标：展开/收起时切换左右箭头码位
    Splitter* splitter_ = nullptr;
    dui::Panel* panel_ = nullptr;
    dui::Text* title_ = nullptr;
    dui::Panel* dirBar_ = nullptr;             // 连线箭头方向栏（仅连线可见）
    dui::Icon* dirRadios_[3] = {};             // 三个单选项的圆点图标
    dui::Text* dirLabels_[3] = {};             // 三个单选项的文字
    dui::Scroller* scroller_ = nullptr;
    dui::RichEditor* editor_ = nullptr;

    Kind kind_ = Kind::None;
    int64_t detailId_ = 0;
    int64_t dirLineId_ = 0;                    // 当前方向栏对应的连线 id
    int direction_ = 0;
    dui::richtext::RichDoc doc_;
    dui::String lastSavedJson_;
    bool dirty_ = false;
    bool expanded_ = false;
    bool hasKnowledge_ = false;
    float width_ = 600.f;
    float dragBase_ = 0.f;

    std::function<void()> onToggle_;
    std::function<void()> onResize_;
    std::function<void()> onWidthCommitted_;
    std::function<bool(dui::String&)> pickImage_;
    std::function<void(int64_t, int)> onLineDirection_;
};
