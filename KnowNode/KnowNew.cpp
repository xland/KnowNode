#include "KnowNew.h"

#include "Db/KnowList.h"
#include "Util.h"

KnowNew::KnowNew(DoneFn done, int64_t listId, const dui::String& initial)
    : dui::Window(dui::WindowOptions{
          .title = listId != 0 ? L"修改知识名称" : L"新建知识",
          .width = 380.f,
          .height = 120.f,
      }),
      listId_(listId), initial_(initial), done_(std::move(done))
{
}

void KnowNew::OnCreate()
{
    auto* root = Root();
    root->SetStyle(dui::Style()
        .FlexDirection(dui::FlexDirection::Column)
        .Padding(dui::Thickness(20.f, 18.f))
        .Gap(14.f)
        .Background(0xFFFFFFFF));

    input_ = root->AddChild<dui::TextBox>(dui::Style()
        .Height(32.f).Padding(dui::Thickness(0.f, 8.f))
        .FontSize(14.f).Color(0xFF1F2329)
        .Background(0xFFFFFFFF)
        .BorderWidth(1.f).BorderColor(0xFFD9DCE1).CornerRadius(4.f));
    input_->OnSubmit([this] { Confirm(); });

    auto* row = root->AddChild<dui::Panel>(dui::Style()
        .FlexDirection(dui::FlexDirection::Row)
        .JustifyContent(dui::Justify::End).Gap(10.f));
    auto* cancel = row->AddChild<dui::Button>(L"取消", dui::Style()
        .Padding(dui::Thickness(6.f, 14.f)).Color(0xFF4B5563)
        .CornerRadius(4.f).Cursor(dui::Cursor::Hand));
    cancel->OnClick([this](dui::PointerEvent&) { Close(); });
    auto* ok = row->AddChild<dui::Button>(L"确定", dui::Style()
        .Padding(dui::Thickness(6.f, 14.f)).Color(0xFFFFFFFF)
        .Background(0xFF2563EB).CornerRadius(4.f).Cursor(dui::Cursor::Hand));
    ok->OnClick([this](dui::PointerEvent&) { Confirm(); });

    input_->SetText(initial_);
    input_->Focus();
    input_->SelectAll();
}

void KnowNew::Confirm()
{
    const dui::String name = input_->GetText();
    if (name.empty()) return;
    if (listId_ != 0)
    {
        db::KnowList::instance().rename(listId_, dui::WideToUtf8(name));
        resultId_ = listId_;
    }
    else
        resultId_ = db::KnowList::instance().add(dui::WideToUtf8(name));
    Close();
}

bool KnowNew::OnClosing()
{
    if (done_) done_(resultId_);
    if (closed_) closed_();
    return true;
}
