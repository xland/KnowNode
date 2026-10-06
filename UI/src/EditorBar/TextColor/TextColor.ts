import html from "./TextColor.html?raw";
import textColorSvg from "../icon/textColor.svg?raw";
import CtrlBase from "../../CtrlBase";
import { setTextColor } from "roosterjs-content-model-api";
import EditorContent from "../../EditorContent/EditorContent";
import Msg from "../../Msg";

const HEX_COLOR = /^#[0-9a-fA-F]{6}$/;

class TextColor extends CtrlBase {
  private input!: HTMLInputElement;

  constructor() {
    super(html);
  }

  override ready(): void {
    this.dom.querySelector(".toolIcon")!.innerHTML = textColorSvg;
    this.input = this.dom.querySelector('input[type="color"]')!;

    // 取色器持续拖动时实时应用颜色；也覆盖系统取色面板点“确定”的场景
    this.input.addEventListener("input", () => {
      setTextColor(EditorContent.editor, this.input.value);
    });

    // 光标/选区处的当前文字颜色同步到取色器，打开面板时能看到当前值
    Msg.on("editorState", (state) => {
      const color = state.textColor;
      if (color && HEX_COLOR.test(color)) {
        this.input.value = color;
      }
    });
  }
}

export default new TextColor();
