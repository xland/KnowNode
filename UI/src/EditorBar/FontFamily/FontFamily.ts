import html from "./FontFamily.html?raw";
import CtrlBase from "../../CtrlBase";
import { setFontName } from "roosterjs-content-model-api";
import EditorContent from "../../EditorContent/EditorContent";
import Msg from "../../Msg";

/** 下拉可选的字体，展示顺序即列表顺序 */
const FONT_OPTIONS = ["微软雅黑", "宋体", "黑体", "仿宋", "楷体", "Arial", "Times New Roman", "Courier New"];

/**
 * 字体选择器（模块单例）。
 * 根元素即工具栏按钮；下拉列表在展开时用 DOM 构建挂到 body，关闭即移除。
 */
class FontFamily extends CtrlBase {
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
    Msg.on("editorState", (state) => this.render(state.fontName));
  }

  /** 用当前字体渲染按钮标签与下拉选中态（编辑器给的 fontFamily 可能带引号/回退列表，取首个并去引号） */
  private render(fontName: string): void {
    this.current = (fontName ?? "").split(",")[0].trim().replace(/^["']|["']$/g, "");
    this.dom.querySelector<HTMLElement>(".fontName").textContent = this.current || "默认";
  }

  private buildPopup(): HTMLDivElement {
    const popup = document.createElement("div");
    popup.className = "fontDropdown";
    for (const name of FONT_OPTIONS) {
      const item = document.createElement("div");
      item.className = name === this.current ? "fontItem selected" : "fontItem";
      item.dataset.value = name;
      item.textContent = name;
      item.style.fontFamily = name;
      popup.appendChild(item);
    }
    return popup;
  }

  private onPicked(value: string): void {
    setFontName(EditorContent.editor, value);
    this.render(value);
    this.close();
  }
}

export default new FontFamily();
