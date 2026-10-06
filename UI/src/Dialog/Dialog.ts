import "./Dialog.scss";
import html from "./Dialog.html?raw";
import CtrlBase from "../CtrlBase";

/** prompt() 的入参 */
interface PromptOptions {
  /** 标题栏文字 */
  title: string;
  /** 输入框上方的说明文字；不传就不显示这一行 */
  label?: string;
  /** 输入框初始值（重命名时回填旧名字） */
  value?: string;
  placeholder?: string;
  okText?: string;
  cancelText?: string;
  /** 输入长度上限；按 Unicode 码点计数（见 Arch/33 的标题规则） */
  maxLength?: number;
}

/**
 * 通用悬浮对话框（DOM 实现，不是原生对话框），目前只提供「一个输入框 + 确定/取消」这一种形态，
 * 够新建知识与重命名知识用（见 Arch/34）。
 *
 * 关闭的三种结果：
 * - 确定 / 回车 → resolve 输入框的 trim 后内容（空串也是合法结果，是否允许由调用方校验）；
 * - 取消 / Esc / 点遮罩 → resolve null。
 *
 * 首次使用时才挂到 body：这个对话框多数会话里根本不会出现，没必要启动就建 DOM。
 */
class Dialog extends CtrlBase {
  private mounted = false;
  private resolveFn: ((value: string | null) => void) | null = null;

  constructor() {
    super(html);
  }

  override ready(): void {
    this.dom.addEventListener("pointerdown", (e) => {
      // 点在遮罩上（不在对话框本体里）= 取消
      if (e.target === this.dom) this.close(null);
    });
    this.dom.querySelector<HTMLElement>(".dialogCancel")!.addEventListener("click", () => this.close(null));
    this.dom.querySelector<HTMLElement>(".dialogOk")!.addEventListener("click", () => this.close(this.inputValue()));
    this.input.addEventListener("keydown", (e) => {
      if (e.key === "Enter") this.close(this.inputValue());
      else if (e.key === "Escape") this.close(null);
    });
  }

  prompt(options: PromptOptions): Promise<string | null> {
    if (!this.mounted) {
      this.appendTo(document.body);
      this.mounted = true;
    }
    this.dom.querySelector<HTMLElement>(".dialogTitle")!.textContent = options.title;

    const label = this.dom.querySelector<HTMLElement>(".dialogLabel")!;
    label.textContent = options.label ?? "";
    label.style.display = options.label ? "" : "none";

    this.input.value = options.value ?? "";
    this.input.placeholder = options.placeholder ?? "";
    this.input.maxLength = options.maxLength ?? 36;
    this.dom.querySelector<HTMLElement>(".dialogOk")!.textContent = options.okText ?? "确定";
    this.dom.querySelector<HTMLElement>(".dialogCancel")!.textContent = options.cancelText ?? "取消";

    this.dom.classList.add("show");
    this.input.focus();
    this.input.select();

    return new Promise<string | null>((resolve) => {
      // 上一次没关闭就又弹了一次：先把旧的那个以 null 结掉，别让它的 Promise 永远悬着
      this.resolveFn?.(null);
      this.resolveFn = resolve;
    });
  }

  private get input(): HTMLInputElement {
    return this.dom.querySelector<HTMLInputElement>(".dialogInput")!;
  }

  private inputValue(): string {
    return this.input.value.trim();
  }

  private close(value: string | null): void {
    this.dom.classList.remove("show");
    const resolve = this.resolveFn;
    this.resolveFn = null;
    resolve?.(value);
  }
}

export default new Dialog();
