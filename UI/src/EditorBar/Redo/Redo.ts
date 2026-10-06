import { createButton } from "../../ToolbarButton";
import { redo } from "roosterjs-content-model-core";
import redoSvg from "../icon/redo.svg?raw";

export const redoButton = createButton({
  icon: redoSvg,
  title: "重做 (Ctrl+Y)",
  onClick: redo,
  isDisabled: (state) => !state.canRedo,
});