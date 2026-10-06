import { createButton } from "../../ToolbarButton";
import { toggleUnderline } from "roosterjs-content-model-api";
import underlineSvg from "../icon/underline.svg?raw";

export const underlineButton = createButton({
  icon: underlineSvg,
  title: "下划线 (Ctrl+U)",
  onClick: toggleUnderline,
  isChecked: (state) => state.isUnderline === true,
});