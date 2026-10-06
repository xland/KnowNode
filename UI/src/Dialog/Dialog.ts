import "./Dialog.scss";
import html from "./Dialog.html?raw";
import CtrlBase from "../CtrlBase";

/** prompt() 的入参 */
interface PromptOptions {
  /** 标题栏文字；不传就不显示标题栏（锚定形态只要输入框与两个按钮） */
  title?: string;
  /** 输入框上方的说明文字；不传就不显示这一行 */
  label?: string;
  /** 输入框初始值（重命名时回填旧名字） */
  value?: string;
  placeholder?: string;
  okText?: string;
  cancelText?: string;
  /** 输入长度上限；按 Unicode 码点计数（见 Arch/33 的标题规则） */
  maxLength?: number;
  /**
   * 传了就是**锚定形态**：浮层贴在它下方、**没有遮罩**（见 Dialog.scss 的 .anchored）。
   * 不传是居中的模态形态（带遮罩），点遮罩 = 取消。
   */
  anchor?: HTMLElement;
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
  /** 锚定形态下贴着哪个元素；非锚定（居中带遮罩）时是 null */
  private anchor: HTMLElement | null = null;

  constructor() {
    super(html);
  }

  override ready(): void {
    this.dom.addEventListener("pointerdown", (e) => {
      // 点在遮罩上（不在对话框本体里）= 取消。锚定形态没有遮罩，点外面归下面的文档级监听管
      if (!this.anchor && e.target === this.dom) this.close(null);
    });
    // 锚定形态：点到别处就收掉。挂在捕获阶段，先于各面板自己的处理。
    // 点在锚点元素上**故意不管**——那是"再点一次按钮"，由调用方自己决定收起还是再开
    document.addEventListener("pointerdown", (e) => {
      if (!this.isOpen() || !this.anchor) return;
      const target = e.target as Node;
      if (this.dom.contains(target) || this.anchor.contains(target)) return;
      this.close(null);
    }, true);
    // 失焦与改窗口大小都收掉：浮层是按屏幕坐标摆的，窗口一变就错位了
    window.addEventListener("blur", () => {
      if (this.anchor) this.close(null);
    });
    window.addEventListener("resize", () => {
      if (this.anchor) this.close(null);
    });
    this.dom.querySelector<HTMLElement>(".dialogCancel")!.addEventListener("click", () => this.close(null));
    this.dom.querySelector<HTMLElement>(".dialogOk")!.addEventListener("click", () => this.close(this.inputValue()));
    this.input.addEventListener("keydown", (e) => {
      if (e.key === "Enter") this.close(this.inputValue());
      else if (e.key === "Escape") this.close(null);
    });
  }

  /**
   * 弹窗是不是正开着（供调用方做"再点一次按钮 = 收起"）。
   * 必须先判 mounted：dom 是**首次 prompt 才挂上去**的（这个对话框多数会话根本不出现），
   * 之前没弹过就问它开没开，this.dom 还是 undefined。
   */
  isOpen(): boolean {
    return this.mounted && this.dom.classList.contains("show");
  }

  /** 外部主动收起（按取消处理，Promise 拿到 null）；没挂上去就不用收 */
  dismiss(): void {
    if (this.mounted) this.close(null);
  }

  prompt(options: PromptOptions): Promise<string | null> {
    if (!this.mounted) {
      this.appendTo(document.body);
      this.mounted = true;
    }
    this.anchor = options.anchor ?? null;
    this.dom.classList.toggle("anchored", this.anchor != null);

    const titleEl = this.dom.querySelector<HTMLElement>(".dialogTitle")!;
    titleEl.textContent = options.title ?? "";
    titleEl.style.display = options.title ? "" : "none";

    const label = this.dom.querySelector<HTMLElement>(".dialogLabel")!;
    label.textContent = options.label ?? "";
    label.style.display = options.label ? "" : "none";

    this.input.value = options.value ?? "";
    this.input.placeholder = options.placeholder ?? "";
    this.input.maxLength = options.maxLength ?? 36;
    this.dom.querySelector<HTMLElement>(".dialogOk")!.textContent = options.okText ?? "确定";
    this.dom.querySelector<HTMLElement>(".dialogCancel")!.textContent = options.cancelText ?? "取消";

    this.dom.classList.add("show");
    // 先显示再按实际尺寸定位：offsetWidth / offsetHeight 只有显示后才有值
    if (this.anchor) this.placeUnder(this.anchor);
    this.input.focus();
    this.input.select();

    return new Promise<string | null>((resolve) => {
      // 上一次没关闭就又弹了一次：先把旧的那个以 null 结掉，别让它的 Promise 永远悬着
      this.resolveFn?.(null);
      this.resolveFn = resolve;
    });
  }

  /** 锚定形态：浮层左缘对齐锚点左缘、贴在它下方 6px；贴近视口下/右边缘时往回收，别被裁掉 */
  private placeUnder(anchor: HTMLElement): void {
    const box = this.dom.querySelector<HTMLElement>(".dialog")!;
    const rect = anchor.getBoundingClientRect();
    const left = Math.max(4, Math.min(rect.left, window.innerWidth - box.offsetWidth - 4));
    const top = Math.max(4, Math.min(rect.bottom + 6, window.innerHeight - box.offsetHeight - 4));
    box.style.left = `${left}px`;
    box.style.top = `${top}px`;
  }

  private get input(): HTMLInputElement {
    return this.dom.querySelector<HTMLInputElement>(".dialogInput")!;
  }

  private inputValue(): string {
    return this.input.value.trim();
  }

  private close(value: string | null): void {
    this.dom.classList.remove("show");
    this.dom.classList.remove("anchored");
    this.anchor = null;
    const resolve = this.resolveFn;
    this.resolveFn = null;
    resolve?.(value);
  }
}

export default new Dialog();
