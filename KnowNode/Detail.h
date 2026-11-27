#pragma once

#include <functional>

#include "DUI/DUI.h"
#include "Splitter.h"

// 右侧详情面板：封装可拖拽分隔条、标题/工具栏/富文本编辑器，
// 以及文档内容的加载、编辑与保存。与窗口的布局联动通过回调注入。
//
// 面板只服务「节点」与「连线」两种对象（整篇知识没有自己的详情）；
// 面板随选中显隐：选中即展开，没选中则收起。
class Detail final : public dui::Panel
{
public:
    enum class Kind { None, Node, Line };

    Detail();

    // entityId：节点 id / 连线 id，标题提交时用于写库。
    // 切换实体会先提交当前标题，再更新 detailId_/entityId_。
    void Show(Kind kind, int64_t detailId, int64_t entityId, const dui::String& title);
    // 清空内容：落盘当前编辑 → 丢掉当前对象 → 面板收起。
    void Clear();
    void Flush();

    Kind CurrentKind() const { return kind_; }
    /// 当前有没有内容可显示（选中了节点或连线）
    bool HasContent() const { return kind_ != Kind::None; }
    void SetTitle(const dui::String& title);
    void UpdateTitleIf(Kind kind, int64_t detailId, const dui::String& title);

    void SetWidth(float width);
    float Width() const { return width_; }
    void ApplyLayout();

    /// 面板占位变了（拖分隔条、内容切换导致显隐）→ 外层重排三栏
    void OnResize(std::function<void()> fn) { onResize_ = std::move(fn); }
    void OnWidthCommitted(std::function<void()> fn) { onWidthCommitted_ = std::move(fn); }
    void SetPickImage(std::function<bool(dui::String&)> fn) { pickImage_ = std::move(fn); }
    void SetImageSrcMapper(dui::RichEditor::ImageSrcMapper fn)
    {
        editor_->SetImageSrcMapper(std::move(fn));
    }
    /// 连线箭头方向变更：回调参数为 (lineId, direction: 0=A→B 1=B→A 2=双向)
    void OnLineDirection(std::function<void(int64_t, int)> fn) { onLineDirection_ = std::move(fn); }
    /// 标题提交：回调参数为 (kind, entityId, text)，仅 Node/Line 触发
    void OnTitleCommit(std::function<void(Kind, int64_t, const dui::String&)> fn)
    {
        onTitleCommit_ = std::move(fn);
    }
    /// 选中连线时设定方向栏上下文；kind 非 Line 时方向栏隐藏。
    void ShowDirection(int64_t lineId, int direction);

private:
    void CommitTitle();
    void SetDirection(int direction, bool notify = true);
    void AppendBlock(dui::richtext::BlockKind kind);
    void InsertImageFile();
    void MarkDirty();
    void LoadDoc(int64_t detailId);
    void UpdateInlineStyleButtons(const dui::richtext::InlineStyleState& state);

    Splitter* splitter_ = nullptr;
    dui::Panel* panel_ = nullptr;
    dui::TextBox* title_ = nullptr;
    dui::Panel* dirBar_ = nullptr;             // 连线箭头方向栏（仅连线可见）
    dui::Icon* dirRadios_[3] = {};             // 三个单选项的圆点图标
    dui::Text* dirLabels_[3] = {};             // 三个单选项的文字
    dui::Scroller* scroller_ = nullptr;
    dui::RichEditor* editor_ = nullptr;
    dui::Panel* inlineStyleBtns_[4] = {};          // 粗体/斜体/下划线/删除线，按光标或选区样式高亮

    Kind kind_ = Kind::None;
    int64_t detailId_ = 0;
    int64_t entityId_ = 0;                     // 当前标题对应的节点 / 连线 id
    dui::String lastTitle_;                    // 本次 Show 时的标题，用于判断是否需要提交
    int64_t dirLineId_ = 0;                    // 当前方向栏对应的连线 id
    int direction_ = 0;
    dui::richtext::RichDoc doc_;
    dui::String lastSavedJson_;
    bool dirty_ = false;
    float width_ = 600.f;
    float dragBase_ = 0.f;

    std::function<void()> onResize_;
    std::function<void()> onWidthCommitted_;
    std::function<bool(dui::String&)> pickImage_;
    std::function<void(int64_t, int)> onLineDirection_;
    std::function<void(Kind, int64_t, const dui::String&)> onTitleCommit_;
};
