import "./ContentBox.scss";
import html from "./ContentBox.html?raw";
import CtrlBase from "../CtrlBase";
import ArticleTitle from "../ArticleTitle/ArticleTitle";
import ArticleEditor from "../ArticleEditor/ArticleEditor";

class ContentBox extends CtrlBase {
  /** 与 ContentBox.scss 中 .splitter 宽度一致 */
  private static readonly SPLITTER_WIDTH = 4;
  /** 各面板最小宽度，须与各面板 scss 的 min-width 保持一致 */
  private static readonly ARTICLE_TITLE_MIN_WIDTH = 200; // ArticleTitle.scss
  private static readonly ARTICLE_EDITOR_MIN_WIDTH = 200; // ArticleEditor.scss

  constructor() {
    super(html);
  }

  override ready(): void {
    ArticleTitle.appendTo(this.dom);
    ArticleEditor.appendTo(this.dom);

    // 两条 splitter 绝对定位，需手动贴到各自目标面板右边缘
    this.syncSplitterPositions();

    // 绑定拖拽
    for (const splitter of this.dom.querySelectorAll<HTMLElement>(".splitter")) {
      splitter.addEventListener("pointerdown", (e) => this.startDrag(splitter, e));
    }
  }

  /** 把每条 splitter 贴到其 data-target 面板的右边缘并水平居中 */
  private syncSplitterPositions(): void {
    const boxLeft = this.dom.getBoundingClientRect().left;
    const half = ContentBox.SPLITTER_WIDTH / 2;
    for (const splitter of this.dom.querySelectorAll<HTMLElement>(".splitter")) {
      const panel = this.dom.querySelector<HTMLElement>(`#${splitter.dataset.target}`);
      if (!panel) continue;
      const rect = panel.getBoundingClientRect();
      splitter.style.left = `${rect.left - boxLeft + rect.width - half}px`;
    }
  }

  private startDrag(splitter: HTMLElement, e: PointerEvent): void {
    e.preventDefault();
    const panel = this.dom.querySelector<HTMLElement>(`#${splitter.dataset.target}`);
    const startX = e.clientX;
    const startWidth = panel.getBoundingClientRect().width;
    const minWidth = ContentBox.ARTICLE_TITLE_MIN_WIDTH;
    // 可拖到的最大宽度 = 容器宽 - 其它固定面板宽 - 弹性面板最小宽
    const otherFixedWidth = this.fixedPanelWidths(splitter.dataset.target ?? "");
    const editorMinWidth = ContentBox.ARTICLE_EDITOR_MIN_WIDTH;
    const maxWidth = Math.max(minWidth, this.dom.getBoundingClientRect().width - otherFixedWidth - editorMinWidth);

    const onMove = (ev: PointerEvent) => {
      const width = Math.min(maxWidth, Math.max(minWidth, startWidth + ev.clientX - startX));
      panel.style.width = `${width}px`;
      this.syncSplitterPositions();
    };

    const onUp = () => {
      document.removeEventListener("pointermove", onMove);
      document.removeEventListener("pointerup", onUp);
      document.body.classList.remove("splitter-dragging");
    };

    document.body.classList.add("splitter-dragging");
    document.addEventListener("pointermove", onMove);
    document.addEventListener("pointerup", onUp);
  }

  /** 除目标面板外，其它固定宽度面板的当前总宽 */
  private fixedPanelWidths(targetId: string): number {
    let total = 0;
    for (const id of ["articleTitle"]) {
      if (id === targetId) continue;
      const el = this.dom.querySelector<HTMLElement>(`#${id}`);
      if (el) total += el.getBoundingClientRect().width;
    }
    return total;
  }
}

export default new ContentBox();
