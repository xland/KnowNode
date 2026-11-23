#include "IconFont.h"

#include <windows.h>

const wchar_t* IconFont::Family()
{
    // 族名必须活到进程结束（Style::FontFamily 只存指针，不拷字符串）。
    // 用 lambda if/else，不用三元——MSVC 对「函数内 static + 非平凡类型 + 三元初始化」
    // 会生成错误代码，结果恒为空串。
    static const dui::String family = []() -> dui::String {
        // 资源类型必须是 RT_RCDATA 宏（MAKEINTRESOURCE(10)），不是字符串 L"RT_RCDATA"——
        // 后者是自定义类型名，会找不到资源 → 族名为空 → 图标全变方框 □。
        HRSRC res = FindResourceW(nullptr, L"IDR_ICONFONT", RT_RCDATA);
        if (res == nullptr) return {};
        HGLOBAL mem = LoadResource(nullptr, res);
        if (mem == nullptr) return {};
        const void* data = LockResource(mem);
        const DWORD size = SizeofResource(nullptr, res);
        if (data == nullptr || size == 0) return {};
        return dui::LoadFontFamilyFromMemory(data, size);
    }();
    return family.c_str();
}

dui::Text* IconFont::CreateIcon(dui::Panel* parent, wchar_t glyph, const dui::Style& style)
{
    const wchar_t text[2] = { glyph, L'\0' };
    // TextAlign 默认是 Start（左对齐）：图标字形必须显式设 Center，才能在盒子内水平居中。
    return parent->AddChild<dui::Text>(text,
        dui::Style(style).FontFamily(Family()).TextAlign(dui::TextAlign::Center));
}

void IconFont::SetGlyph(dui::Text* text, wchar_t glyph)
{
    const wchar_t ch[2] = { glyph, L'\0' };
    text->SetText(ch);
}
