import html from "./Link.html?raw";
import linkSvg from "../icon/link.svg?raw";
import CtrlBase from "../../CtrlBase";
import { adjustLinkSelection, insertLink } from "roosterjs-content-model-api";
import EditorContent from "../../EditorContent/EditorContent";
import Msg from "../../Msg";

/**
 * 插入/编辑链接按钮（模块单例），与官方 insertLinkButton 对齐。
 *
 * 官方这个按钮没有任何可点性门禁（不定义 isDisabled / isChecked），永远可点：
 * “当前有没有选中文本”完全交给点击后的 adjustLinkSelection(editor) 处理 ——
 * 光标折叠时它会自动扩选一个词，光标/选区在链接里时扩成整条链接。
 * 因此这里也不做任何选区探测，只用原生 canUnlink 做按钮高亮。
 *
 * 弹窗与官方 showInputDialog 的两个输入项一致：
 * - 链接地址（URL）：initValue = adjustLinkSelection 返回的第 2 项（已有链接地址）
 * - 显示文本（Display as）：initValue = 返回的第 1 项（选中文本 / 链接文本）
 * 并复刻官方的联动：地址被修改、且显示文本未被单独改过（仍等于改动前的地址）时，显示文本跟随地址。
 *
 * 弹窗 DOM 写在 Link.html 中，全生命周期只存在一份，开合靠 hidden 属性切换显隐。
 */
class Link extends CtrlBase {
  private popup: HTMLDivElement | null = null;
  private urlInput: HTMLInputElement | null = null;
  private displayInput: HTMLInputElement | null = null;
  /** 弹窗打开时的初始值，用于判断用户是否真的改过（与官方提交条件一致） */
  private initUrl = "";
  private initDisplayText = "";
  /** 地址框上一次的值，用于「改地址时显示文本跟随」的联动判断 */
  private lastUrl = "";

  constructor() {
    super(html);
  }

  override ready(): void {
    this.dom.querySelector(".toolIcon")!.innerHTML = linkSvg;

    // 弹窗与按钮同出一份模板（见 Link.html：弹窗在前、按钮在最后），这里把弹窗挪到 body：
    // 1) 脱离 #editorBar 的 user-select: none，否则输入框里没法用鼠标选中文字；
    // 2) 保证 position: fixed 以视口为参照，不受工具栏祖先影响。
    const popup = this.dom.previousElementSibling as HTMLDivElement;
    const urlInput = popup.querySelector<HTMLInputElement>(".linkDialogUrl")!;
    const displayInput = popup.querySelector<HTMLInputElement>(".linkDialogDisplay")!;
    document.body.appendChild(popup);
    this.popup = popup;
    this.urlInput = urlInput;
    this.displayInput = displayInput;

    // 与官方 showInputDialog 的 onItemChange 同款联动：改地址时，若显示文本未被单独改过
    // （仍等于改动前的地址），则显示文本跟随新地址
    urlInput.addEventListener("input", () => {
      if (displayInput.value === this.lastUrl) {
        displayInput.value = urlInput.value;
      }
      this.lastUrl = urlInput.value;
    });

    popup.addEventListener("click", (e) => {
      const btn = (e.target as HTMLElement).closest<HTMLButtonElement>("button.linkDialogBtn");
      if (!btn) {
        return;
      }
      if (btn.classList.contains("linkDialogBtnPrimary")) {
        this.confirm();
      } else {
        this.close();
      }
    });

    popup.addEventListener("keydown", (e) => {
      if (e.key === "Enter") {
        e.preventDefault();
        this.confirm();
      } else if (e.key === "Escape") {
        this.close();
      }
    });

    // 阻止 mousedown 默认行为，避免按钮抢走编辑器焦点/清掉选区
    this.dom.addEventListener("mousedown", (e) => e.preventDefault());
    this.dom.addEventListener("click", () => (this.isOpen() ? this.close() : this.open()));
    // 与官方一致：按钮始终可点，只按原生 canUnlink 决定是否高亮
    Msg.on("editorState", (state) => {
      this.dom.classList.toggle("active", state.canUnlink === true);
    });
  }

  /** 弹窗当前是否可见（hidden 属性只由 open/close 维护） */
  private isOpen(): boolean {
    return !this.popup!.hidden;
  }

  private open(): void {
    const editor = EditorContent.editor;
    // 与官方 insertLinkButton 一致：先 adjustLinkSelection，一次拿到选中文本与已有链接地址；
    // 光标折叠在链接内时它会把选区扩成整条链接，insertLink 才能“更新”该链接而不是插入新词
    const [displayText, url] = adjustLinkSelection(editor);
    this.initUrl = url ?? "";
    this.initDisplayText = displayText;
    this.lastUrl = this.initUrl;

    const popup = this.popup!;
    const urlInput = this.urlInput!;
    const displayInput = this.displayInput!;
    urlInput.value = this.initUrl;
    displayInput.value = this.initDisplayText;

    // 先显示再定位：hidden 状态下 offsetHeight 为 0，量出来的高度是错的
    popup.hidden = false;
    this.placePopup(popup);
    document.addEventListener("mousedown", this.onDocMouseDown);
    window.addEventListener("blur", this.close);
    // 与官方一致：默认聚焦地址栏
    urlInput.focus();
    urlInput.select();
  }

  private placePopup(popup: HTMLDivElement): void {
    const rect = this.dom.getBoundingClientRect();
    const below = rect.bottom + 4;
    const popupHeight = popup.offsetHeight;
    popup.style.left = `${rect.left}px`;
    popup.style.top =
      below + popupHeight > window.innerHeight ? `${rect.top - popupHeight - 4}px` : `${below}px`;
  }

  private confirm(): void {
    const url = this.urlInput?.value.trim() ?? "";
    const displayText = this.displayInput?.value ?? "";
    // 与官方提交条件一致：地址为空，或用户什么都没改 → 不执行，直接关闭
    if (!url || (url === this.initUrl && displayText === this.initDisplayText)) {
      this.close();
      return;
    }
    const editor = EditorContent.editor;
    // 恢复编辑器焦点（会还原失焦前选区，即 open() 中 adjustLinkSelection 的结果）
    editor.focus();
    // 官方调用形态：insertLink(editor, link, anchorTitle, displayText)，不指定 target
    insertLink(editor, url, url, displayText);
    this.close();
  }

  private close = (): void => {
    this.popup!.hidden = true;
    document.removeEventListener("mousedown", this.onDocMouseDown);
    window.removeEventListener("blur", this.close);
  };

  /** 仅在展开期间绑定，因此触发时弹层必然存在 */
  private onDocMouseDown = (e: MouseEvent) => {
    const target = e.target as Node;
    if (!this.dom.contains(target) && !this.popup!.contains(target)) {
      this.close();
    }
  };
}

export default new Link();
