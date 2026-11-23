#pragma once

#include "DUI/DUI.h"

// 项目自带的 iconfont：码位定义在 iconfont.json，字体以 exe 资源嵌入。
// 用字体图标渲染时统一走这里——族名必须活到进程结束（Style::FontFamily 只存指针），
// 且图标字号要足够大，否则 PUA 字形会糊成一团。
class IconFont
{
public:
    // 图标码位（来自 iconfont.json）
    static constexpr wchar_t ArrowLeft = 0xE709;    // 详情展开（向左）
    static constexpr wchar_t ArrowRight = 0xE70E;   // 详情收起（向右）
    static constexpr wchar_t Center = 0xEB4E;       // 回到中心（取景框）
    static constexpr wchar_t Add = 0xE798;          // 新建知识
    static constexpr wchar_t Nodes = 0xE613;        // 节点（标题栏 logo）

    // 富文本工具栏（第二批）
    static constexpr wchar_t ListBullet = 0xE600;
    static constexpr wchar_t Code = 0xE601;
    static constexpr wchar_t Quote = 0xE602;
    static constexpr wchar_t Image = 0xE603;

    // 富文本工具栏（第三批：行内格式 + 分割线）
    static constexpr wchar_t Underline = 0xE604;
    static constexpr wchar_t Strike = 0xE605;
    static constexpr wchar_t Italic = 0xE606;
    static constexpr wchar_t HorizonLine = 0xE607;
    static constexpr wchar_t Bold = 0xE608;

    // 字体族名，进程级 static，活到进程结束。加载失败返回空串。
    static const wchar_t* Family();

    // 创建一个字体图标 Text：自动套用族名，字号/颜色/尺寸由调用方给的 style 决定。
    // glyph 传上面的码位常量，style 里必须带 FontSize。
    static dui::Text* CreateIcon(dui::Panel* parent, wchar_t glyph, const dui::Style& style);
    // 切换已有图标 Text 的码位（如展开/收起箭头）
    static void SetGlyph(dui::Text* text, wchar_t glyph);
};
