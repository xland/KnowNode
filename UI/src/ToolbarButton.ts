import CtrlBase from "./CtrlBase";
import EditorContent from "./EditorContent/EditorContent";
import Msg from "./Msg";
import type { ContentModelFormatState, IEditor } from "roosterjs-content-model-types";

/**
 * 工具栏按钮声明。
 * 对标官方 Ribbon 的 RibbonButton（key/iconName/unlocalizedText/onClick/isChecked/isDisabled），
 * 去掉与 Fluent UI 图标、本地化相关的字段，其余语义保持一致。
 *
 * 新增按钮只需写一条声明，以下交互由 createButton 统一处理：
 * - 阻止 mousedown 默认行为，避免按钮抢走编辑器焦点/清掉选区
 * - 点击时把编辑器实例传给原生命令
 * - 按 isChecked / isDisabled 联动点亮与禁用态
 */
export interface ToolbarButton {
  /** 图标 SVG 源码 */
  icon: string;
  /** 悬停提示，惯例带上快捷键，如 "加粗 (Ctrl+B)" */
  title: string;
  /** 点击执行的原生命令；onClick 只传 editor，需要额外参数时才包一层 lambda */
  onClick: (editor: IEditor) => void;
  /** 点亮条件，对应 RibbonButton.isChecked */
  isChecked?: (state: ContentModelFormatState) => boolean;
  /** 禁用条件，对应 RibbonButton.isDisabled */
  isDisabled?: (state: ContentModelFormatState) => boolean;
}

class ToolbarButtonCtrl extends CtrlBase {
  private readonly button: ToolbarButton;

  constructor(button: ToolbarButton) {
    // 声明了禁用条件的按钮（撤销/重做/移除链接）初始置灰，待首次 editorState 广播后按实际状态刷新
    super(
      `<button type="button" class="toolBtn"${button.isDisabled ? " disabled" : ""} title="${button.title}"><span class="toolIcon"></span></button>`,
    );
    this.button = button;
  }

  override ready(): void {
    this.dom.querySelector(".toolIcon")!.innerHTML = this.button.icon;
    // roosterjs 的 toggle 类命令按“当前选区”判定 on/off，选区一丢可能被误判（如列表被重复嵌套）
    this.dom.addEventListener("mousedown", (e) => e.preventDefault());
    this.dom.addEventListener("click", () => this.button.onClick(EditorContent.editor));
    if (this.button.isChecked || this.button.isDisabled) {
      Msg.on("editorState", (state) => {
        if (this.button.isChecked) {
          this.dom.classList.toggle("active", this.button.isChecked(state));
        }
        if (this.button.isDisabled) {
          (this.dom as HTMLButtonElement).disabled = this.button.isDisabled(state);
        }
      });
    }
  }
}

/** 按声明创建工具栏按钮 */
export function createButton(button: ToolbarButton): CtrlBase {
  return new ToolbarButtonCtrl(button);
}
