import "./TitleBar.scss";
import html from "./TitleBar.html?raw";
import CtrlBase from "../CtrlBase";
import Msg from "../Msg";
import ArticleTitle from "../ArticleTitle/ArticleTitle";

class TitleBar extends CtrlBase {
  constructor() {
    super(html);
  }

  override ready(): void {
    this.dom.querySelector<HTMLElement>("#titleLabel").addEventListener("mousedown", () => Msg.invoke("hittest", { val: 2 }));
    this.dom.querySelector<HTMLElement>("#minimizeBtn").addEventListener("mousedown", this.onMinimize);
    this.dom.querySelector<HTMLElement>("#restoreBtn").addEventListener("mousedown", () => Msg.invoke("restore"));
    this.dom.querySelector<HTMLElement>("#maximizeBtn").addEventListener("mousedown", () => Msg.invoke("maximize"));
    this.dom.querySelector<HTMLElement>("#closeBtn").addEventListener("mousedown", this.onClose);
    Msg.on("maximize", () => this.syncMaximizeBtn(false));
    Msg.on("restore", () => this.syncMaximizeBtn(true));
  }

  private readonly onMinimize = async () => {
    const minimizeBtn = this.dom.querySelector<HTMLElement>("#minimizeBtn");
    if (!minimizeBtn) return;
    minimizeBtn.classList.add("suppressHover");
    try {
      await Msg.invoke("minimize");
    } finally {
      window.addEventListener("mousemove", () => minimizeBtn.classList.remove("suppressHover"), { once: true });
    }
  };

  /**
   * 关窗：先把还没落库的改动写完再 close。
   * window.close() 一路走到 native 的 WM_CLOSE → 窗口销毁，WebView 随之中断，
   * 飞在半路的写库 IPC 会被掐掉——所以这里 await 完 flush 再关，
   * 否则最后 2 秒内的编辑会随窗口一起消失。
   */
  private readonly onClose = async () => {
    await ArticleTitle.flush();
    window.close();
  };

  private syncMaximizeBtn(restored: boolean): void {
    const restoreBtn = this.dom.querySelector<HTMLElement>("#restoreBtn");
    const maximizeBtn = this.dom.querySelector<HTMLElement>("#maximizeBtn");
    if (restoreBtn) restoreBtn.style.display = restored ? "none" : "flex";
    if (maximizeBtn) maximizeBtn.style.display = restored ? "flex" : "none";
  }
}

export default new TitleBar();
