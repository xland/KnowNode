import "./Code.scss";
import html from "./Code.html?raw";
import codeSvg from "../icon/code.svg?raw";
import CtrlBase from "../../CtrlBase";
import Msg from "../../Msg";
import EditorContent from "../../EditorContent/EditorContent";
import { CODE_LANGS, DEFAULT_LANG, type CodeLangId } from "../../CodeHighlight";
import { codeOf, insertCodeBlock, updateCodeBlock } from "../../EditorContent/CodeBlock";

/**
 * 插入 / 编辑代码块按钮（模块单例）。
 *
 * 弹窗（Code.html）与链接弹窗同款：全生命周期一份，挂到 body 上，靠 hidden 切换显隐、居中显示。
 * 关窗条件与链接弹窗有一点不同：链接弹窗挂了 window blur（点别处/切窗口就收），
 * 这里不挂——代码多半要从别的窗口复制过来，切出去再回来时弹窗得还在。
 * 两条打开路径：
 * - 点工具栏按钮：空着打开，确定后在光标处插入一个代码块；
 * - 双击正文里已有的代码块：带出它的代码与语言，确定后整块重建（可改语言）。
 *   （EditorContent 只认出双击命中了哪个代码块，然后广播 editCodeBlock，由这里接手。）
 *
 * 代码块本体是 rooster 的只读 entity，着色由 shiki 在插入/重建时算好（见 CodeHighlight）。
 */
class Code extends CtrlBase {
  private dialog!: HTMLElement;
  private input!: HTMLTextAreaElement;
  private lang!: HTMLSelectElement;

  /** 正在改的代码块外壳；null 表示这次是插入新的 */
  private editing: HTMLElement | null = null;

  constructor() {
    super(html);
  }

  override ready(): void {
    this.dom.querySelector(".toolIcon")!.innerHTML = codeSvg;

    // 与 Link 同款：弹窗挪到 body —— 避开工具栏的 user-select: none（否则没法用鼠标选代码），
    // 也让 fixed 定位以视口为参照
    const dialog = this.dom.previousElementSibling as HTMLElement;
    document.body.appendChild(dialog);
    this.dialog = dialog;
    this.input = dialog.querySelector<HTMLTextAreaElement>(".codeDialogInput")!;
    this.lang = dialog.querySelector<HTMLSelectElement>(".codeDialogLang")!;
    for (const item of CODE_LANGS) {
      const option = document.createElement("option");
      option.value = item.id;
      option.textContent = item.name;
      this.lang.appendChild(option);
    }

    dialog.addEventListener("click", (e) => {
      const btn = (e.target as HTMLElement).closest<HTMLElement>("button.codeDialogBtn");
      if (!btn) return;
      if (btn.classList.contains("codeDialogBtnPrimary")) {
        this.confirm();
      } else {
        this.close();
      }
    });

    // Esc 取消；Enter 留给代码里的换行，确定只能点按钮（或 Ctrl+Enter）
    dialog.addEventListener("keydown", (e) => {
      if (e.key === "Escape") {
        this.close();
      } else if (e.key === "Enter" && e.ctrlKey) {
        this.confirm();
      }
    });

    // 阻止 mousedown 默认行为，避免按钮抢走编辑器焦点/清掉选区
    this.dom.addEventListener("mousedown", (e) => e.preventDefault());
    this.dom.addEventListener("click", () => this.openForInsert());
    Msg.on("editCodeBlock", (data) => this.openForEdit((data as { wrapper: HTMLElement }).wrapper));
  }

  /** 工具栏按钮：插入新的 */
  private openForInsert(): void {
    this.editing = null;
    this.input.value = "";
    this.lang.value = DEFAULT_LANG;
    this.show();
  }

  /** 双击已有代码块：带出它现在的代码与语言 */
  private openForEdit(wrapper: HTMLElement): void {
    const { code, lang } = codeOf(wrapper);
    this.editing = wrapper;
    this.input.value = code;
    this.lang.value = lang;
    this.show();
  }

  private show(): void {
    this.dialog.hidden = false;
    // 只监听"在本窗口里点了弹窗外面"：不像下拉/链接弹窗那样再挂 window blur。
    // 代码经常要从别的窗口（IDE、网页）复制过来，切出去时弹窗必须留着
    document.addEventListener("mousedown", this.onDocMouseDown);
    this.input.focus();
    this.input.select();
  }

  private close = (): void => {
    this.dialog.hidden = true;
    this.editing = null;
    document.removeEventListener("mousedown", this.onDocMouseDown);
  };

  /** 仅在弹窗打开期间绑定，因此触发时弹窗必然可见 */
  private onDocMouseDown = (e: MouseEvent): void => {
    const target = e.target as Node;
    if (!this.dom.contains(target) && !this.dialog.contains(target)) {
      this.close();
    }
  };

  private confirm(): void {
    // 只去掉结尾的空行：代码里的前导空格与内部空行都是内容
    const code = this.input.value.replace(/\s+$/, "");
    const lang = this.lang.value as CodeLangId;
    const wrapper = this.editing;
    this.close();
    if (!code) return;

    const editor = EditorContent.editor;
    // 弹窗抢走了焦点，先还给编辑器：insertEntity 要按光标位置插
    editor.focus();
    if (wrapper) {
      updateCodeBlock(editor, wrapper, code, lang);
      // 直接改 DOM 不走 rooster 的命令，它不会自己发 contentChanged，这里补一条让保存生效
      Msg.emit("editorContentChanged");
    } else {
      insertCodeBlock(editor, code, lang);
    }
  }
}

export default new Code();
