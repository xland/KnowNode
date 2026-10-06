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
 */
class WindowBorder extends CtrlBase {
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
        Msg.invoke("hittest", { val });
      });
    }
  }
}

export default new WindowBorder();