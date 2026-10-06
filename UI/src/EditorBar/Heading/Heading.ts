import html from "./Heading.html?raw";
import CtrlBase from "../../CtrlBase";
import { setHeadingLevel } from "roosterjs-content-model-api";
import EditorContent from "../../EditorContent/EditorContent";
import Msg from "../../Msg";

type HeadingLevel = 0 | 1 | 2 | 3 | 4 | 5 | 6;

/** 列表项：普通段落(0)放最后，一级~六级标题在前（展示顺序即下拉顺序） */
const HEADING_ITEMS: { level: HeadingLevel; label: string }[] = [
  { level: 1, label: "一级标题" },
  { level: 2, label: "二级标题" },
  { level: 3, label: "三级标题" },
  { level: 4, label: "四级标题" },
  { level: 5, label: "五级标题" },
  { level: 6, label: "六级标题" },
  { level: 0, label: "普通段落" },
];

const LABEL_OF: Record<HeadingLevel, string> = {
  0: "普通段落",
  1: "一级标题",
  2: "二级标题",
  3: "三级标题",
  4: "四级标题",
  5: "五级标题",
  6: "六级标题",
};

/**
 * 标题/段落下拉（模块单例）。
 * 下拉列表内容：一级~六级标题（h1~h6）+ 普通段落。
 * - 选中某项：光标/选区所在段落设为对应标题级别（0 表示恢复普通段落）
 * - 按钮文字随光标所在段的标题级别联动（来自 getFormatState 的 headingLevel）
 */
class Heading extends CtrlBase {
  private current: HeadingLevel = 0;

  /** 挂到 body 上的下拉弹层；null 表示当前未展开 */
  private popup: HTMLDivElement | null = null;

  // ---- 弹层开合 / 定位 / 外部关闭 ----
  // 注意：close、onItemClick、onDocMouseDown 必须是箭头函数类字段。
  // addEventListener / removeEventListener 靠引用相等解绑，改成普通方法会静默解绑失败
  // （表现为弹层能开、关不掉，且 document 上的监听每次展开都会多挂一个）。

  /** 点击按钮时调用：已展开则收起，否则展开 */
  private toggle(): void {
    this.popup ? this.close() : this.open();
  }

  private open(): void {
    const popup = this.buildPopup();
    // 先挂载再量高度：挂载前 offsetHeight 为 0，会导致“向上翻折”判断失效
    document.body.appendChild(popup);
    this.position(popup);
    popup.addEventListener("click", this.onItemClick);
    document.addEventListener("mousedown", this.onDocMouseDown);
    window.addEventListener("blur", this.close);
    this.popup = popup;
  }

  private close = (): void => {
    this.popup?.remove();
    this.popup = null;
    document.removeEventListener("mousedown", this.onDocMouseDown);
    window.removeEventListener("blur", this.close);
  };

  /** 按触发按钮位置定位弹层；下方放不下则向上翻折 */
  private position(popup: HTMLElement): void {
    const rect = this.dom.getBoundingClientRect();
    const below = rect.bottom + 4;
    const popupHeight = popup.offsetHeight;
    popup.style.left = `${rect.left}px`;
    popup.style.top =
      below + popupHeight > window.innerHeight ? `${rect.top - popupHeight - 4}px` : `${below}px`;
  }

  /** 事件委托：点中带 data-value 的列表项交给 onPicked */
  private onItemClick = (e: MouseEvent): void => {
    const item = (e.target as HTMLElement).closest<HTMLElement>("[data-value]");
    if (item) {
      this.onPicked(item.dataset.value ?? "");
    }
  };

  /** 仅在展开期间绑定，因此触发时弹层必然存在 */
  private onDocMouseDown = (e: MouseEvent): void => {
    const target = e.target as Node;
    if (!this.dom.contains(target) && !this.popup!.contains(target)) {
      this.close();
    }
  };

  constructor() {
    super(html);
  }

  override ready(): void {
    this.dom.addEventListener("click", () => this.toggle());
    Msg.on("editorState", (state) => this.render(state.headingLevel as number | undefined));
  }

  /** 同步按钮标签与下拉选中态：0 / 无 = 普通段落 */
  private render(headingLevel?: number): void {
    const level =
      headingLevel && headingLevel >= 1 && headingLevel <= 6
        ? (headingLevel as HeadingLevel)
        : 0;
    this.current = level;
    this.dom.querySelector<HTMLElement>(".fontName").textContent = LABEL_OF[level];
  }

  private buildPopup(): HTMLDivElement {
    const popup = document.createElement("div");
    popup.className = "fontDropdown headingDropdown";
    for (const item of HEADING_ITEMS) {
      const row = document.createElement("div");
      row.className = item.level === this.current ? "fontItem selected" : "fontItem";
      row.dataset.value = String(item.level);
      row.textContent = item.label;
      popup.appendChild(row);
    }
    return popup;
  }

  private onPicked(value: string): void {
    const level = Number(value) as HeadingLevel;
    setHeadingLevel(EditorContent.editor, level);
    this.render(level);
    this.close();
  }
}

export default new Heading();
