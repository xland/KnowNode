import { createHighlighterCoreSync } from "shiki/core";
import { createJavaScriptRegexEngine } from "shiki/engine/javascript";
// 语法与主题都按需静态导入（shiki 的 exports 把 shiki/langs/* 映射到 dist/langs/*，所以要带 .mjs）
import githubLight from "shiki/themes/github-light.mjs";
import c from "shiki/langs/c.mjs";
import cpp from "shiki/langs/cpp.mjs";
import css from "shiki/langs/css.mjs";
import go from "shiki/langs/go.mjs";
import html from "shiki/langs/html.mjs";
import java from "shiki/langs/java.mjs";
import javascript from "shiki/langs/javascript.mjs";
import python from "shiki/langs/python.mjs";
import rust from "shiki/langs/rust.mjs";
import sql from "shiki/langs/sql.mjs";
import typescript from "shiki/langs/typescript.mjs";
import xml from "shiki/langs/xml.mjs";

/**
 * 代码着色：只服务"编辑器里的代码块"这一条路（见 `EditorContent/CodeBlock.ts`），
 * 三个时机各着色一次——插入、改代码 / 改语言后重建、打开知识详情时把库里的纯文本重新着色。
 *
 * （2026-10-08 更正：这里原写着"发布到微信时再着色一次"，那条路已经没有了——发布链路连同
 * `WeiXinHtml.ts` 一并删除，现在**没有任何**发布相关的调用方，本文件剩下的导出全归编辑器用。）
 *
 * 用 shiki 而不是 highlight.js / prismjs：它产出的是**内联 style**（`<span style="color:#d73a49">`），
 * 颜色就写在 HTML 里，不依赖任何外部样式表。代码块是塞进 rooster 的只读 entity，
 * 内联样式跟着这段 HTML 走，rooster 重写 DOM、出库入库转几道手都掉不了色；
 * hljs / prism 是 class + 主题 CSS 的路子，得额外挂一份样式表并让它一直活到渲染时。
 *
 * 同步构造（createHighlighterCoreSync + JS 正则引擎）：语法与主题都是静态导入的，
 * 引擎不用加载 wasm，所以不用 await——插入、打开都能直接拿到着色结果。
 */

/** 主题：github-light，与正文白底协调 */
const THEME = "github-light";

/** 支持的语言：id 就是 shiki 的语法名，也原样写进正文的 data-lang */
export const CODE_LANGS = [
  { id: "typescript", name: "TypeScript" },
  { id: "javascript", name: "JavaScript" },
  { id: "html", name: "HTML" },
  { id: "css", name: "CSS" },
  { id: "cpp", name: "C++" },
  { id: "c", name: "C" },
  { id: "python", name: "Python" },
  { id: "rust", name: "Rust" },
  { id: "go", name: "Go" },
  { id: "java", name: "Java" },
  { id: "sql", name: "SQL" },
  { id: "xml", name: "XML" },
] as const;

export type CodeLangId = (typeof CODE_LANGS)[number]["id"];

/** 认不出语言（老数据、手改过）时按它处理 */
export const DEFAULT_LANG: CodeLangId = "typescript";

const highlighter = createHighlighterCoreSync({
  themes: [githubLight],
  langs: [c, cpp, css, go, html, java, javascript, python, rust, sql, typescript, xml],
  engine: createJavaScriptRegexEngine(),
});

/** 代码 → 着色后的 HTML 字符串（<pre …><code><span style="color:…">…） */
export function highlightCode(code: string, lang: CodeLangId): string {
  return highlighter.codeToHtml(code, { lang, theme: THEME });
}

/** 字符串是不是支持的语言 id */
export function isCodeLang(value: string | undefined): value is CodeLangId {
  return CODE_LANGS.some((item) => item.id === value);
}
