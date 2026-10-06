import html from "./FontSize.html?raw";
import CtrlBase from "../../CtrlBase";
import { setFontSize } from "roosterjs-content-model-api";
import EditorContent from "../../EditorContent/EditorContent";
import Msg from "../../Msg";
import { cssLengthToPx } from "../cssLength";

/** 下拉可选的文字大小（像素 px，整数档位），展示顺序即列表顺序 */
const FONT_SIZES = [10, 11, 12, 14, 15, 16, 18, 20, 24, 28, 36, 48, 72];

/**
 * 字号选择器（模块单例）。
 * 根元素即工具栏按钮；下拉列表在展开时用 DOM 构建挂到 body，关闭即移除。
 * 与 FontFamily 同构，仅值来源不同：以 px 设置/展示文字大小。
 * 注意 editorState.fontSize 由 roosterjs 统一折算成 pt（如 "11.25pt" 对应 15px），
 * 显示前需换算回 px。
 */
class FontSize extends CtrlBase {
  /** 当前文字大小（px 展示文本，如 "15px"），空串表示编辑器未给出字号 */
  private current = "";

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
    popup.style.top =
      below + popupHeight > window.innerHeight ? `${rect.top - popupHeight - 4}px` : `${below}px`;
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
    this.dom.addEventListener("click", () => this.toggle());
    Msg.on("editorState", (state) => this.render(state.fontSize));
  }

  /** 用当前字号渲染按钮标签与下拉选中态 */
  private render(fontSize: string): void {
    const px = cssLengthToPx(fontSize);
    this.current = Number.isNaN(px) ? "" : `${Math.round(px)}px`;
    this.dom.querySelector<HTMLElement>(".fontName").textContent = this.current || "默认";
  }

  private buildPopup(): HTMLDivElement {
    const popup = document.createElement("div");
    popup.className = "fontDropdown sizeDropdown";
    for (const size of FONT_SIZES) {
      const item = document.createElement("div");
      const text = `${size}px`;
      item.className = this.current === text ? "fontItem selected" : "fontItem";
      item.dataset.value = text;
      item.textContent = text;
      popup.appendChild(item);
    }
    return popup;
  }

  private onPicked(value: string): void {
    setFontSize(EditorContent.editor, value);
    this.render(value);
    this.close();
  }
}

export default new FontSize();
