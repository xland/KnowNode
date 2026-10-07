import "./ContentBox.scss";
import html from "./ContentBox.html?raw";
import CtrlBase from "../CtrlBase";
import Msg from "../Msg";
import KnowList from "../KnowList/KnowList";
import KnowNet from "../KnowNet/KnowNet";
import KnowDetail from "../KnowDetail/KnowDetail";

/** 与 ContentBox.scss 里 .splitter 的宽度一致 */
const SPLITTER_WIDTH = 4;
/** 三个元素的最小宽度（Arch/31）：列表 300、画布 500；详情的最小值待确认，先取 300 */
const KNOW_LIST_MIN = 300;
const KNOW_NET_MIN = 500;
const KNOW_DETAIL_MIN = 300;
/** 拖出来的宽度存 setting 表（Arch/40），下次打开还是这个宽度 */
const LIST_WIDTH_KEY = "know_list.width";
const DETAIL_WIDTH_KEY = "know_detail.width";

/**
 * 容器区（模块单例，见 Arch/31）：横向三块 —— 知识列表 / 节点画布 / 详细内容。
 *
 * 三块并排成三栏：详情那一栏**默认收起**（不占宽度），用户点 `#detailToggle` 才展开。
 *
 * 两块 splitter：
 * - 列表右边缘那条拖列表宽度（画布拿剩下的）；
 * - 详情左边缘那条拖详情宽度。
 * 二者都是改"被拖的那个面板"的宽度，另一边由 flex 自动吃掉剩余空间。
 *
 * 展开按钮与两条 splitter 都是绝对定位的，面板尺寸一变（展开 / 收起、拖过、窗口拉伸）
 * 就要重新贴一次——统一在 syncSplitterPositions() 里做。
 */
class ContentBox extends CtrlBase {
  constructor() {
    super(html);
  }

  override ready(): void {
    KnowList.appendTo(this.dom);
    KnowNet.appendTo(this.dom);
    KnowDetail.appendTo(this.dom);

    void this.restoreWidths();

    for (const splitter of this.dom.querySelectorAll<HTMLElement>(".splitter")) {
      splitter.addEventListener("pointerdown", (e) => this.startDrag(splitter, e));
    }

    // 详情那一栏展开 / 收起（用户点按钮），它那条 splitter 与那个按钮都要重新贴边
    Msg.on("knowDetailExpanded", () => this.syncSplitterPositions());
    this.dom.querySelector<HTMLElement>("#detailToggle")!.addEventListener("click", () => void KnowDetail.toggle());
    // 窗口拉伸会让面板宽度自己变（画布是弹性的），splitter 得跟着走
    new ResizeObserver(() => this.syncSplitterPositions()).observe(this.dom);
  }

  /** 上次拖出来的宽度：从 setting 里读回来 */
  private async restoreWidths(): Promise<void> {
    try {
      const listWidth = Number(await Msg.invoke("setting.get", { key: LIST_WIDTH_KEY, fallback: "" }));
      if (listWidth >= KNOW_LIST_MIN) this.panel("knowList").style.width = `${listWidth}px`;
      const detailWidth = Number(await Msg.invoke("setting.get", { key: DETAIL_WIDTH_KEY, fallback: "" }));
      if (detailWidth >= KNOW_DETAIL_MIN) this.panel("knowDetail").style.width = `${detailWidth}px`;
    } catch {
      // 读不出来就用 scss 里的默认宽度
    } finally {
      this.syncSplitterPositions();
    }
  }

  private panel(id: string): HTMLElement {
    return this.dom.querySelector<HTMLElement>(`#${id}`)!;
  }

  /** 把每条 splitter 贴到它目标面板的对应边缘（并水平居中到那条边上） */
  private syncSplitterPositions(): void {
    const boxLeft = this.dom.getBoundingClientRect().left;
    const half = SPLITTER_WIDTH / 2;
    for (const splitter of this.dom.querySelectorAll<HTMLElement>(".splitter")) {
      const panel = this.panel(splitter.dataset.target ?? "");
      if (!panel) continue;
      // 详情面板没显示（没选中任何东西）时，它那条 splitter 也跟着藏起来
      if (splitter.dataset.target === "knowDetail" && !panel.classList.contains("show")) {
        splitter.style.display = "none";
        continue;
      }
      splitter.style.display = "";
      const rect = panel.getBoundingClientRect();
      const edge = splitter.dataset.edge === "left" ? rect.left : rect.right;
      splitter.style.left = `${edge - boxLeft - half}px`;
    }
    this.syncToggle();
  }

  /**
   * 展开按钮：收起时贴在容器右边缘，展开时贴到详情面板的左边缘外侧（所以它要往左挪一个面板宽度）。
   * 图标方向也跟着换：收起时是 icon-expand-left（点它往左展开），展开时是 icon-expand-right。
   */
  private syncToggle(): void {
    const toggle = this.dom.querySelector<HTMLElement>("#detailToggle")!;
    const expanded = KnowDetail.isExpanded;
    const width = expanded ? this.panel("knowDetail").getBoundingClientRect().width : 0;
    toggle.style.right = `${width}px`;
    toggle.classList.toggle("icon-expand-right", expanded);
    toggle.classList.toggle("icon-expand-left", !expanded);
    toggle.title = expanded ? "收起详细内容" : "展开详细内容";
    // 没打开任何知识：既没有节点也没有"知识本身"可显示，按钮也就没什么可展开的
    toggle.style.visibility = KnowDetail.canExpand ? "" : "hidden";
  }

  private startDrag(splitter: HTMLElement, e: PointerEvent): void {
    e.preventDefault();
    const targetId = splitter.dataset.target ?? "";
    const panel = this.panel(targetId);
    // 拖左边缘（详情）时鼠标右移是变窄，所以方向取反
    const dir = splitter.dataset.edge === "left" ? -1 : 1;
    const startX = e.clientX;
    const startWidth = panel.getBoundingClientRect().width;
    const boxWidth = this.dom.getBoundingClientRect().width;
    const min = targetId === "knowList" ? KNOW_LIST_MIN : KNOW_DETAIL_MIN;
    // 拖到头也不能把另一边挤没：画布始终要留住 KNOW_NET_MIN
    const reserved =
      targetId === "knowList" ? KNOW_NET_MIN + this.visibleDetailWidth() : KNOW_LIST_MIN + KNOW_NET_MIN;
    const max = Math.max(min, boxWidth - reserved);

    const onMove = (ev: PointerEvent) => {
      const width = Math.min(max, Math.max(min, startWidth + dir * (ev.clientX - startX)));
      panel.style.width = `${width}px`;
      this.syncSplitterPositions();
    };
    const onUp = () => {
      document.removeEventListener("pointermove", onMove);
      document.removeEventListener("pointerup", onUp);
      document.body.classList.remove("splitter-dragging");
      void this.saveWidth(targetId, panel.getBoundingClientRect().width);
    };

    document.body.classList.add("splitter-dragging");
    document.addEventListener("pointermove", onMove);
    document.addEventListener("pointerup", onUp);
  }

  /** 详情栏只有展开时才占布局宽度；收起时它是 display:none，不算进"要留住的空间" */
  private visibleDetailWidth(): number {
    return KnowDetail.isExpanded ? this.panel("knowDetail").getBoundingClientRect().width : 0;
  }

  private async saveWidth(targetId: string, width: number): Promise<void> {
    const key = targetId === "knowList" ? LIST_WIDTH_KEY : DETAIL_WIDTH_KEY;
    await Msg.invoke("setting.set", { key, value: String(Math.round(width)) }).catch(() => {});
  }
}

export default new ContentBox();
