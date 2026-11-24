#include "IconFont.h"

const wchar_t* IconFont::Family()
{
    return dui::LoadFontFamilyFromResource(L"IDR_ICONFONT");
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
