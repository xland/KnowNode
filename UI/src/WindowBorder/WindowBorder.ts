import "./WindowBorder.scss";
import html from "./WindowBorder.html?raw";
import CtrlBase from "../CtrlBase";
import Msg from "../Msg";

/**
 * 主窗口的自绘窗口边框 + 8 个角点（给 WS_POPUP 主窗口用的，替代 WS_OVERLAPPEDWINDOW
 * 自带的可拖动边框/角点）。
 *
 * 8 个 DOM 节点的 id 与 Win32 HT_* 的对应（Window::hittest 会把它作为 WM_NCLBUTTONDOWN 的
 * wParam PostMessage 出去，由系统进入标准调整大小流程）：
 *   borderLeft          → HTLEFT          (10)
 *   borderRight         → HTRIGHT         (11)
 *   borderTop           → HTTOP           (12)
 *   cornerTopLeft       → HTTOPLEFT       (13)
 *   cornerTopRight      → HTTOPRIGHT      (14)
 *   borderBottom        → HTBOTTOM        (15)
 *   cornerBottomLeft    → HTBOTTOMLEFT    (16)
 *   cornerBottomRight   → HTBOTTOMRIGHT   (17)
 *
 * 模块单例，由 Main.ts 挂到 body。z-index 999 浮在所有内容之上；最大化时被遮住、不需要命中。
 *
 * **最大化时这 8 个触发区集体失效**（2026-10-07）：最大化窗口拖不动边框，留着它们只会
 * 让光标在边缘变成调整大小的样式、点了还没反应。失效靠两道——CSS 摘掉 `pointer-events`
 * （鼠标不再命中、光标也不再变），TS 里再拦一道（免得将来改了样式又把拖拽放出来）。
 */
class WindowBorder extends CtrlBase {
  /**
   * 窗口是不是最大化的。初值 true：C++ 侧 `Window::show()` 用的是 `SW_SHOWMAXIMIZED`，
   * 一上来就是最大化；之后由 `maximize` / `restore` 事件（C++ 的 WM_SIZE 广播）切换。
   */
  private maximized = true;

  constructor() {
    super(html);
  }

  override ready(): void {
    const triggers: Array<[string, number]> = [
      ["borderLeft", 10],
      ["borderRight", 11],
      ["borderTop", 12],
      ["cornerTopLeft", 13],
      ["cornerTopRight", 14],
      ["borderBottom", 15],
      ["cornerBottomLeft", 16],
      ["cornerBottomRight", 17],
    ];
    for (const [id, val] of triggers) {
      this.dom.querySelector<HTMLElement>(`#${id}`)!.addEventListener("mousedown", () => {
        if (this.maximized) return; // 最大化：不进调整大小的流程（CSS 那道已挡住命中，这是第二道）
        Msg.invoke("win.hittest", { val });
      });
    }

    // 最大化 / 还原由 C++ 的 WM_SIZE 广播（见 Window::onSize），与 TitleBar 那两个按钮同源
    Msg.on("maximize", () => this.setResizable(false));
    Msg.on("restore", () => this.setResizable(true));
    this.setResizable(!this.maximized); // 按初始状态刷一次 class
  }

  /** 开 / 关这 8 个触发区的拖拽能力（最大化时关） */
  private setResizable(resizable: boolean): void {
    this.maximized = !resizable;
    this.dom.classList.toggle("maximized", !resizable);
  }
}

export default new WindowBorder();