import { createButton } from "../../ToolbarButton";
import { toggleSuperscript } from "roosterjs-content-model-api";
import superscriptSvg from "../icon/superscript.svg?raw";

export const superscriptButton = createButton({
  icon: superscriptSvg,
  title: "上标",
  onClick: toggleSuperscript,
  isChecked: (state) => state.isSuperscript === true,
});