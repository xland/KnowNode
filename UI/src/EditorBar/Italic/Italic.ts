import { createButton } from "../../ToolbarButton";
import { toggleItalic } from "roosterjs-content-model-api";
import italicSvg from "../icon/italic.svg?raw";

export const italicButton = createButton({
  icon: italicSvg,
  title: "斜体 (Ctrl+I)",
  onClick: toggleItalic,
  isChecked: (state) => state.isItalic === true,
});