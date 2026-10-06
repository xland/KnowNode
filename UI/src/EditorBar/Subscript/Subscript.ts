import { createButton } from "../../ToolbarButton";
import { toggleSubscript } from "roosterjs-content-model-api";
import subscriptSvg from "../icon/subscript.svg?raw";

export const subscriptButton = createButton({
  icon: subscriptSvg,
  title: "下标",
  onClick: toggleSubscript,
  isChecked: (state) => state.isSubscript === true,
});