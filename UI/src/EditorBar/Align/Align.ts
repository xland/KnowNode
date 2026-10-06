import html from "./Align.html?raw";
import alignLeftSvg from "../icon/alignLeft.svg?raw";
import alignCenterSvg from "../icon/alignCenter.svg?raw";
import alignRightSvg from "../icon/alignRight.svg?raw";
import alignJustifySvg from "../icon/alignJustify.svg?raw";
import CtrlBase from "../../CtrlBase";
import { setAlignment } from "roosterjs-content-model-api";
import EditorContent from "../../EditorContent/EditorContent";
import Msg from "../../Msg";

type AlignValue = "left" | "center" | "right" | "justify";

/** 段落水平对齐选项，展示顺序即列表顺序 */
const ALIGN_OPTIONS = [
  { value: "left", label: "左对齐", svg: alignLeftSvg },
  { value: "center", label: "居中对齐", svg: alignCenterSvg },
  { value: "right", label: "右对齐", svg: alignRightSvg },
  { value: "justify", label: "两端对齐", svg: alignJustifySvg },
];

function isAlign(value: string): value is AlignValue {
  return ALIGN_OPTIONS.some((o) => o.value === value);
}

/**
 * 对齐方式下拉（模块单例）。
 * 按钮显示当前对齐图标，点击展开挂到 body 的下拉；选择后应用并对编辑器状态回显。
 */
class Align extends CtrlBase {
  private current: AlignValue = "left";

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
    Msg.on("editorState", (state) => {
      if (isAlign(state.textAlign)) {
        this.render(state.textAlign);
      }
    });
    this.render(this.current);
  }

  /** 把当前对齐方式渲染到按钮图标 */
  private render(value: AlignValue): void {
    this.current = value;
    const option = ALIGN_OPTIONS.find((o) => o.value === value)!;
    const icon = this.dom.querySelector<HTMLElement>(".toolIcon")!;
    icon.innerHTML = option.svg;
    icon.title = option.label;
  }

  private buildPopup(): HTMLDivElement {
    const popup = document.createElement("div");
    popup.className = "fontDropdown alignDropdown";
    for (const o of ALIGN_OPTIONS) {
      const item = document.createElement("div");
      item.className = o.value === this.current ? "alignItem selected" : "alignItem";
      item.dataset.value = o.value;
      const icon = document.createElement("span");
      icon.className = "alignItemIcon";
      icon.title = o.label; // 名称做进图标的 title
      icon.innerHTML = o.svg;
      item.append(icon);
      popup.appendChild(item);
    }
    return popup;
  }

  private onPicked(value: string): void {
    if (isAlign(value)) {
      setAlignment(EditorContent.editor, value);
      this.render(value);
    }
    this.close();
  }
}

export default new Align();
