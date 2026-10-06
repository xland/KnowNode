import { createButton } from "../../ToolbarButton";
import { toggleNumbering } from "roosterjs-content-model-api";
import listNumberSvg from "../icon/listNumber.svg?raw";

export const listNumberButton = createButton({
  icon: listNumberSvg,
  title: "有序列表",
  onClick: toggleNumbering,
  isChecked: (state) => state.isNumbering === true,
});