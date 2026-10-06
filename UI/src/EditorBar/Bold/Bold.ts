import { createButton } from "../../ToolbarButton";
import { toggleBold } from "roosterjs-content-model-api";
import boldSvg from "../icon/bold.svg?raw";

export const boldButton = createButton({
  icon: boldSvg,
  title: "加粗 (Ctrl+B)",
  onClick: toggleBold,
  isChecked: (state) => state.isBold === true,
});