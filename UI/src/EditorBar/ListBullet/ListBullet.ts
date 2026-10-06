import { createButton } from "../../ToolbarButton";
import { toggleBullet } from "roosterjs-content-model-api";
import listBulletSvg from "../icon/listBullet.svg?raw";

export const listBulletButton = createButton({
  icon: listBulletSvg,
  title: "无序列表",
  onClick: toggleBullet,
  isChecked: (state) => state.isBullet === true,
});