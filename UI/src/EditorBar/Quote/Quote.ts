import { createButton } from "../../ToolbarButton";
import { toggleBlockQuote } from "roosterjs-content-model-api";
import type { ContentModelFormatContainerFormat } from "roosterjs-content-model-types";
import quoteSvg from "../icon/quote.svg?raw";

/**
 * 引用的格式：一律留空。
 * 引用长什么样（灰底 + 左侧框线 + 缩进 + 深灰文字）在 EditorContent.scss 里，
 * 作为 #editorContent blockquote 的默认样式，不写成内联 style——
 * 否则每段引用在入库的 HTML 里都挂着同一串重复 style，以后改样子还得回头洗数据。
 *
 * 这些键必须显式写：toggleBlockQuote 自带一套默认格式（margin 1em/40px、padding-left 10px…），
 * 不覆盖就会连 margin 一起写进 style。其中 margin 要传"官方给 blockquote 的默认值"：
 * rooster 拿它跟自己的隐式格式比，一样就判定"没改过"、不往元素上写，
 * 于是 HTML 里只剩下干净的 <blockquote>，缩进由 CSS 说了算。
 * 传 undefined 或空字符串都不行：那样它认为跟隐式格式不同，反而写一堆 margin: 0px 上去。
 */
const QUOTE_FORMAT: ContentModelFormatContainerFormat = {
  marginTop: "1em",
  marginBottom: "1em",
  marginLeft: "40px",
  marginRight: "40px",
  paddingTop: undefined,
  paddingRight: undefined,
  paddingBottom: undefined,
  paddingLeft: undefined,
  borderLeft: undefined,
  backgroundColor: undefined,
  textColor: undefined,
};

export const quoteButton = createButton({
  icon: quoteSvg,
  title: "引用",
  onClick: (editor) => toggleBlockQuote(editor, QUOTE_FORMAT),
  isChecked: (state) => state.isBlockQuote === true,
});
