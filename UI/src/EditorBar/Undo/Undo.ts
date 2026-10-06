import { createButton } from "../../ToolbarButton";
import { undo } from "roosterjs-content-model-core";
import undoSvg from "../icon/undo.svg?raw";

export const undoButton = createButton({
  icon: undoSvg,
  title: "撤销 (Ctrl+Z)",
  onClick: undo,
  isDisabled: (state) => !state.canUndo,
});