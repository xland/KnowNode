#include "TitleBar.h"

#include <windows.h>

namespace
{
    bool LoadSvgResource(dui::Svg* svg, const wchar_t* resourceName)
    {
        HRSRC res = FindResourceW(nullptr, resourceName, RT_RCDATA);
        if (res == nullptr) return false;
        HGLOBAL mem = LoadResource(nullptr, res);
        if (mem == nullptr) return false;
        const void* data = LockResource(mem);
        const DWORD bytes = SizeofResource(nullptr, res);
        if (data == nullptr || bytes == 0) return false;
        return svg->LoadSvgMemory(data, static_cast<uint32_t>(bytes));
    }
}

TitleBar::TitleBar()
{
    SetStyle(dui::Style()
        .Height(32.f).FlexShrink(0.f)
        .FlexDirection(dui::FlexDirection::Row).AlignItems(dui::Align::Center)
        .Background(0xFFFAFAFD)
        .BorderWidth(dui::Thickness(0.f, 0.f, 0.f, 1.f))
        .BorderColor(0xFFD3E3FD)
        .AppRegion(dui::AppRegion::Drag));

    auto* titleLabel = AddChild<dui::Panel>(dui::Style()
        .Height(32.f).FlexGrow(1.f)
        .FlexDirection(dui::FlexDirection::Row).AlignItems(dui::Align::Center)
        .Gap(8.f).Padding(dui::Thickness(12.f, 0.f, 0.f, 0.f))
        .AppRegion(dui::AppRegion::Drag));
    auto* titleLogo = titleLabel->AddChild<dui::Svg>(dui::Style()
        .Width(16.f).Height(16.f).FlexShrink(0.f)
        .AppRegion(dui::AppRegion::Drag));
    LoadSvgResource(titleLogo, L"LOGO_SVG");
    titleLabel->AddChild<dui::Text>(L"KnowNode 知点", dui::Style()
        .FontSize(14.f).Color(0xFF606060)
        .TextOverflow(dui::TextOverflow::Ellipsis)
        .AppRegion(dui::AppRegion::Drag));

    auto titleButtonStyle = [] {
        return dui::Style()
            .Width(36.f).Height(32.f).FlexShrink(0.f)
            .FlexDirection(dui::FlexDirection::Row)
            .JustifyContent(dui::Justify::Center).AlignItems(dui::Align::Center)
            .AppRegion(dui::AppRegion::NoDrag);
    };
    auto iconStyle = [] {
        return dui::Style().Width(12.f).Height(12.f).Color(0xFF999999);
    };

    auto* minBtn = AddChild<dui::Panel>(titleButtonStyle());
    auto* minIcon = minBtn->AddChild<dui::Icon>(iconStyle());
    minIcon->SetIcon(dui::IconName::Minimize);
    minBtn->When(dui::State::Hover, dui::Style().Background(0xFFEEEEEE));
    minBtn->OnPointerEnter([minIcon](dui::PointerEvent&) { minIcon->Set(dui::Prop::Color, 0xFF333333u); });
    minBtn->OnPointerLeave([minIcon](dui::PointerEvent&) { minIcon->Set(dui::Prop::Color, 0xFF999999u); });
    minBtn->OnClick([this](dui::PointerEvent&) { window_->Minimize(); });

    auto* maxBtn = AddChild<dui::Panel>(titleButtonStyle());
    maxIcon_ = maxBtn->AddChild<dui::Icon>(iconStyle());
    maxBtn->When(dui::State::Hover, dui::Style().Background(0xFFEEEEEE));
    maxBtn->OnPointerEnter([this](dui::PointerEvent&) { maxIcon_->Set(dui::Prop::Color, 0xFF333333u); });
    maxBtn->OnPointerLeave([this](dui::PointerEvent&) { maxIcon_->Set(dui::Prop::Color, 0xFF999999u); });
    maxBtn->OnClick([this](dui::PointerEvent&) { window_->MaximizeOrRestore(); });

    auto* closeBtn = AddChild<dui::Panel>(titleButtonStyle());
    auto* closeIcon = closeBtn->AddChild<dui::Icon>(iconStyle());
    closeIcon->SetIcon(dui::IconName::Close);
    closeBtn->When(dui::State::Hover, dui::Style().Background(0xFFE81123));
    closeBtn->OnPointerEnter([closeIcon](dui::PointerEvent&) {
        closeIcon->Set(dui::Prop::Color, 0xFFFFFFFFu);
    });
    closeBtn->OnPointerLeave([closeIcon](dui::PointerEvent&) {
        closeIcon->Set(dui::Prop::Color, 0xFF999999u);
    });
    closeBtn->OnClick([this](dui::PointerEvent&) { window_->Close(); });
}

void TitleBar::Init(dui::Window* window)
{
    window_ = window;
    SyncMaximizeState();
}

void TitleBar::SyncMaximizeState()
{
    maxIcon_->SetIcon(window_->IsMaximized() ? dui::IconName::Restore : dui::IconName::Maximize);
}
