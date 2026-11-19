#include "RichDocStore.h"

#include <utility>

#include "Util.h"

namespace
{
    using dui::String;
    using dui::json::Value;
    namespace rt = dui::richtext;

    const wchar_t* BlockTypeName(rt::BlockKind kind)
    {
        switch (kind)
        {
        case rt::BlockKind::Paragraph: return L"paragraph";
        case rt::BlockKind::Heading:   return L"heading";
        case rt::BlockKind::List:      return L"list";
        case rt::BlockKind::ListItem:  return L"paragraph";
        case rt::BlockKind::Quote:     return L"quote";
        case rt::BlockKind::Code:      return L"codeBlock";
        case rt::BlockKind::Image:     return L"image";
        case rt::BlockKind::Rule:      return L"horizontalRule";
        }
        return L"paragraph";
    }

    String AlignName(rt::Align align)
    {
        switch (align)
        {
        case rt::Align::Center: return L"center";
        case rt::Align::End:    return L"right";
        default:                return L"left";
        }
    }

    String ColorHex(dui::Color color)
    {
        static const wchar_t* digits = L"0123456789abcdef";
        String out = L"#";
        for (int shift = 28; shift >= 0; shift -= 4)
            out += digits[(color >> shift) & 0xF];
        return out;
    }

    void WriteBlockStyle(const rt::BlockStyle& style, Value& block)
    {
        Value s = dui::json::Object();
        if (style.align != rt::Align::Start)
            s.SetString(L"textAlign", AlignName(style.align));
        if (style.lineHeight > 0.f)
            s.Set(L"lineHeight", dui::json::Number(style.lineHeight));
        if (style.lineHeightDip > 0.f)
            s.SetString(L"lineHeight", std::to_wstring(style.lineHeightDip) + L"px");
        if (style.indent > 0.f)
            s.Set(L"indent", dui::json::Number(style.indent));
        if (s.Size() > 0)
            block.Set(L"styles", std::move(s));
    }

    Value RunToJson(const rt::Run& run)
    {
        Value o = dui::json::Object();
        const rt::RunStyle& st = run.style;

        Value styles = dui::json::Object();
        if (!st.family.empty()) styles.SetString(L"fontFamily", st.family);
        if (st.fontSize > 0.f) styles.Set(L"fontSize", dui::json::Number(st.fontSize));
        if (st.weight != 0)    styles.SetInt(L"fontWeight", st.weight);
        if (st.italic)         styles.SetString(L"fontStyle", L"italic");
        if (st.underline || st.strike)
        {
            String deco;
            if (st.underline) deco = L"underline";
            if (st.strike)
            {
                if (!deco.empty()) deco += L" ";
                deco += L"line-through";
            }
            styles.SetString(L"textDecoration", deco);
        }
        if (((st.color >> 24) & 0xFF) != 0)
            styles.SetString(L"color", ColorHex(st.color));
        if (((st.background >> 24) & 0xFF) != 0)
            styles.SetString(L"backgroundColor", ColorHex(st.background));

        if (!st.href.empty())
        {
            o.SetString(L"type", L"hyperlink");
            o.SetString(L"href", st.href);
            o.SetString(L"content", run.text);
        }
        else
        {
            o.SetString(L"type", L"text");
            o.SetString(L"text", run.text);
        }
        if (styles.Size() > 0)
            o.Set(L"styles", std::move(styles));
        return o;
    }

    Value ListToJson(const rt::Block& list);

    Value ItemContentToJson(const rt::Block& item)
    {
        Value content = dui::json::Array();
        for (const rt::Run& run : item.runs)
            content.Push(RunToJson(run));
        for (const rt::Block& child : item.children)
        {
            if (child.kind == rt::BlockKind::List)
                content.Push(ListToJson(child));
        }
        return content;
    }

    Value ListToJson(const rt::Block& list)
    {
        Value o = dui::json::Object();
        o.SetString(L"type", L"list");
        o.SetString(L"listType", list.listType == rt::ListType::Ordered ? L"ordered" : L"unordered");

        Value items = dui::json::Array();
        for (const rt::Block& item : list.children)
        {
            Value one = dui::json::Object();
            one.Set(L"content", ItemContentToJson(item));
            Value s = dui::json::Object();
            if (item.style.indent > 0.f)
                s.Set(L"indent", dui::json::Number(item.style.indent));
            if (s.Size() > 0)
                one.Set(L"styles", std::move(s));
            items.Push(std::move(one));
        }
        o.Set(L"items", std::move(items));
        WriteBlockStyle(list.style, o);
        return o;
    }

    Value BlockToJson(const rt::Block& block)
    {
        if (block.kind == rt::BlockKind::List)
            return ListToJson(block);

        Value o = dui::json::Object();
        o.SetString(L"type", BlockTypeName(block.kind));

        switch (block.kind)
        {
        case rt::BlockKind::Heading:
            o.SetInt(L"level", block.level);
            break;
        case rt::BlockKind::Quote:
            o.SetString(L"attribution", block.attribution);
            break;
        case rt::BlockKind::Code:
            o.SetString(L"code", block.code);
            o.SetBool(L"showLineNumbers", block.showLineNumbers);
            break;
        case rt::BlockKind::Image:
            o.SetString(L"src", block.src);
            o.SetString(L"altText", block.altText);
            if (block.width > 0.f) o.Set(L"width", dui::json::Number(block.width));
            if (block.heightAuto)  o.SetString(L"height", L"auto");
            else if (block.height > 0.f) o.Set(L"height", dui::json::Number(block.height));
            if (block.imageAlign != rt::Align::Start)
                o.SetString(L"alignment", AlignName(block.imageAlign));
            break;
        default:
            break;
        }

        if (block.kind != rt::BlockKind::Code &&
            block.kind != rt::BlockKind::Image &&
            block.kind != rt::BlockKind::Rule)
        {
            Value content = dui::json::Array();
            for (const rt::Run& run : block.runs)
                content.Push(RunToJson(run));
            o.Set(L"content", std::move(content));
        }
        WriteBlockStyle(block.style, o);
        return o;
    }
}

bool RichDocStore::Load(const std::string& utf8Json, dui::richtext::RichDoc& out)
{
    if (!utf8Json.empty())
    {
        dui::richtext::ParseRichDoc(dui::Utf8ToWide(utf8Json.c_str()), out);
        if (!out.blocks.empty()) return true;
    }

    dui::richtext::ParseRichDoc(
        L"{\"settings\":{\"defaultFont\":\"Microsoft YaHei\",\"defaultFontSize\":14},"
        L"\"blocks\":[{\"type\":\"paragraph\",\"content\":\"\"}]}", out);
    return true;
}

dui::String RichDocStore::Save(const dui::richtext::RichDoc& doc)
{
    Value root = dui::json::Object();

    Value settings = dui::json::Object();
    if (!doc.defaultFont.empty()) settings.SetString(L"defaultFont", doc.defaultFont);
    settings.Set(L"defaultFontSize", dui::json::Number(doc.defaultFontSize));
    root.Set(L"settings", std::move(settings));

    Value blocks = dui::json::Array();
    for (const rt::Block& block : doc.blocks)
        blocks.Push(BlockToJson(block));
    root.Set(L"blocks", std::move(blocks));

    return dui::json::Write(root);
}
