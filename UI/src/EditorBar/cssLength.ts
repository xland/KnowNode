/**
 * CSS 长度 → px 数值。
 * roosterjs 通过 editorState 上报的字号 / 行高统一折算成 pt（15px 对应 "11.25pt"），
 * 而工具栏按 px 展示与比较，所以要换算回来：pt→px = ×4/3。
 * @returns px 数值；解析不出长度时返回 NaN，兜底方式由调用方决定
 * （字号选择器显示"默认"，行高选择器落回默认档位）
 */
export function cssLengthToPx(length?: string): number {
  const matched = /^(\d+(?:\.\d+)?)\s*(px|pt)$/i.exec((length ?? "").trim());
  if (!matched) {
    return NaN;
  }
  const value = parseFloat(matched[1]);
  return matched[2].toLowerCase() === "px" ? value : (value * 4) / 3;
}
