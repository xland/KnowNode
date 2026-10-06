import "./StatusBar.scss";
import html from "./StatusBar.html?raw";
import CtrlBase from "../CtrlBase";

/**
 * 底部状态栏（模块单例，见 Arch/30）。
 * 左侧 = 当前选中的是哪个节点，右侧 = 当前知识一共有多少个节点。
 * 两个数字都由 KnowNet 在选中变化 / 加载完一张网时调过来，状态栏自己不发请求。
 */
class StatusBar extends CtrlBase {
  /** 左侧：当前选中的节点 */
  private selection: HTMLElement | null = null;

  /** 右侧：当前知识的节点总数 */
  private count: HTMLElement | null = null;

  constructor() {
    super(html);
  }

  override ready(): void {
    this.selection = this.dom.querySelector<HTMLElement>("#statusSelection");
    this.count = this.dom.querySelector<HTMLElement>("#statusCount");
    this.setSelection("");
    this.setCount(0);
  }

  /** 当前选中的节点标题；传空串 = 没选中任何节点 */
  setSelection(title: string): void {
    if (!this.selection) return;
    this.selection.textContent = title ? `当前选中：${title}` : "未选中节点";
    this.selection.title = title;
  }

  /** 当前知识的节点总数 */
  setCount(count: number): void {
    if (!this.count) return;
    this.count.textContent = `共 ${count} 个节点`;
  }
}

export default new StatusBar();
