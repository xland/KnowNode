import html from "./LineHeight.html?raw";
import lineHeightSvg from "../icon/lineHeight.svg?raw";
import CtrlBase from "../../CtrlBase";
import { getSelectedParagraphs } from "roosterjs-content-model-dom";
import EditorContent from "../../EditorContent/EditorContent";
import Msg from "../../Msg";
import { cssLengthToPx } from "../cssLength";

/** 可选的行高倍数（相对字号，CSS 无单位 line-height），展示顺序即列表顺序 */
const LINE_HEIGHTS = ["1", "1.5", "2", "2.5", "3"];

/** 编辑器正文默认行高：需与 EditorContent.scss 中 #editorContent 的 line-height 保持一致。
 *  段落未显式设置行高时继承该默认值，回显视为选中档位 "2"；
 *  选回这一档时不写内联行高（并摘掉之前设过的），让段落回到继承状态——
 *  默认样式归 CSS，只有用户手动改过的档位才值得写进入库的 HTML。 */
const DEFAULT_LINE_HEIGHT = "2";

/**
 * 行高下拉（模块单例）。
 * 按钮为图标 + 箭头，点开展开挂到 body 的下拉；下拉项为固定倍数，选择后应用并回显选中态。
 */
class LineHeight extends CtrlBase {
  /** 当前行高档位（"1"/"1.5"/…）；初始为默认档位，即下拉默认选中 "2" */
  private current = DEFAULT_LINE_HEIGHT;

  /** 挂到 body 上的下拉弹层；null 表示当前未展开 */
  private popup: HTMLDivElement | null = null;

  // ---- 弹层开合 / 定位 / 外部关闭 ----
  // 注意：close、onItemClick、onDocMouseDown 必须是箭头函数类字段。
  // addEventListener / removeEventListener 靠引用相等解绑，改成普通方法会静默解绑失败
  // （表现为弹层能开、关不掉，且 document 上的监听每次展开都会多挂一个）。

  /** 点击按钮时调用：已展开则收起，否则展开 */
  private toggle(): void {
    this.popup ? this.close() : this.open();
  }

  private open(): void {
    const popup = this.buildPopup();
    // 先挂载再量高度：挂载前 offsetHeight 为 0，会导致“向上翻折”判断失效
    document.body.appendChild(popup);
    this.position(popup);
    popup.addEventListener("click", this.onItemClick);
    document.addEventListener("mousedown", this.onDocMouseDown);
    window.addEventListener("blur", this.close);
    this.popup = popup;
  }

  private close = (): void => {
    this.popup?.remove();
    this.popup = null;
    document.removeEventListener("mousedown", this.onDocMouseDown);
    window.removeEventListener("blur", this.close);
  };

  /** 按触发按钮位置定位弹层；下方放不下则向上翻折 */
  private position(popup: HTMLElement): void {
    const rect = this.dom.getBoundingClientRect();
    const below = rect.bottom + 4;
    const popupHeight = popup.offsetHeight;
    popup.style.left = `${rect.left}px`;
    popup.style.top = below + popupHeight > window.innerHeight ? `${rect.top - popupHeight - 4}px` : `${below}px`;
  }

  /** 事件委托：点中带 data-value 的列表项交给 onPicked */
  private onItemClick = (e: MouseEvent): void => {
    const item = (e.target as HTMLElement).closest<HTMLElement>("[data-value]");
    if (item) {
      this.onPicked(item.dataset.value ?? "");
    }
  };

  /** 仅在展开期间绑定，因此触发时弹层必然存在 */
  private onDocMouseDown = (e: MouseEvent): void => {
    const target = e.target as Node;
    if (!this.dom.contains(target) && !this.popup!.contains(target)) {
      this.close();
    }
  };

  constructor() {
    super(html);
  }

  override ready(): void {
    this.dom.querySelector(".toolIcon")!.innerHTML = lineHeightSvg;
    this.dom.addEventListener("click", () => this.toggle());
    Msg.on("editorState", (state) => this.render(state.lineHeight, state.fontSize));
  }

  /**
   * 记录当前行高档位。
   * roosterjs 上报的 lineHeight 是浏览器折算后的绝对值（如 15px 字号下的默认行高为 "22.5px"），
   * 因此需除以当前字号 fontSize 换算回"相对字号的倍数"再匹配档位；
   * 若拿不到可解析的行高（空文档/未聚焦），按默认档位 2 回显。
   */
  private render(lineHeight?: string, fontSize?: string): void {
    const linePx = cssLengthToPx(lineHeight);
    const fontPx = cssLengthToPx(fontSize);
    if (Number.isNaN(linePx)) {
      this.current = DEFAULT_LINE_HEIGHT;
      return;
    }
    if (Number.isNaN(fontPx)) {
      this.current = "";
      return;
    }

    const factor = linePx / fontPx;
    // 匹配最接近的档位；容差 0.1（换算可能出现极小的浮点/取整误差），其它值不点亮选中态
    let closest: string | null = null;
    for (const item of LINE_HEIGHTS) {
      const diff = Math.abs(parseFloat(item) - factor);
      if (closest === null || diff < Math.abs(parseFloat(closest) - factor)) {
        closest = item;
      }
    }
    this.current = closest && Math.abs(parseFloat(closest) - factor) < 0.1 ? closest : "";
  }

  private buildPopup(): HTMLDivElement {
    const popup = document.createElement("div");
    popup.className = "fontDropdown lineHeightDropdown";
    for (const value of LINE_HEIGHTS) {
      const item = document.createElement("div");
      item.className = value === this.current ? "fontItem selected" : "fontItem";
      item.dataset.value = value;
      item.textContent = value;
      popup.appendChild(item);
    }
    return popup;
  }

  private onPicked(value: string): void {
    const editor = EditorContent.editor;
    editor.focus();
    editor.formatContentModel(
      (model) => {
        const paragraphs = getSelectedParagraphs(model, true);
        if (paragraphs.length === 0) {
          return false;
        }
        paragraphs.forEach((paragraph) => {
          if (value === DEFAULT_LINE_HEIGHT) {
            // 选回默认档位：不写内联行高，让段落从 #editorContent 继承。
            // roosterjs 的行高 applier 只在有值时写 style、不会帮我们清，
            // 所以之前设过别档而留在元素上的 line-height 得自己摘掉，否则它还会压着继承值
            delete paragraph.format.lineHeight;
            paragraph.cachedElement?.style.removeProperty("line-height");
          } else {
            paragraph.format.lineHeight = value;
          }
        });
        return true;
      },
      { apiName: "setLineHeight" },
    );
    // 应用的是下拉档位本身，直接作为选中态（后续 editorState 会按 px 换算回同一档位）
    this.current = value;
    this.close();
  }
}

export default new LineHeight();
