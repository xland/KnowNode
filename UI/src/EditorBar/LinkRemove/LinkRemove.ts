import { createButton } from "../../ToolbarButton";
import { adjustLinkSelection, removeLink } from "roosterjs-content-model-api";
import linkRemoveSvg from "../icon/linkRemove.svg?raw";

export const linkRemoveButton = createButton({
  icon: linkRemoveSvg,
  title: "移除链接",
  onClick: (editor) => {
    // 光标折叠在链接内时先把选区扩到整条链接，removeLink 才能整条移除
    adjustLinkSelection(editor);
    removeLink(editor);
  },
  // 原生 canUnlink：光标/选区命中链接时为 true（折叠光标在链接内同样成立），语义等同“在链接内”
  isChecked: (state) => state.canUnlink === true,
  isDisabled: (state) => state.canUnlink !== true,
});