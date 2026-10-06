import { createButton } from "../../ToolbarButton";
import { toggleStrikethrough } from "roosterjs-content-model-api";
import strikethroughSvg from "../icon/strikethrough.svg?raw";

export const strikethroughButton = createButton({
  icon: strikethroughSvg,
  title: "删除线",
  onClick: toggleStrikethrough,
  isChecked: (state) => state.isStrikeThrough === true,
});