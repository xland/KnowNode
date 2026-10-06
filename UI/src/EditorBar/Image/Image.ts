import { createButton } from "../../ToolbarButton";
import { insertImage } from "roosterjs-content-model-api";
import type { IEditor } from "roosterjs-content-model-types";
import imageSvg from "../icon/image.svg?raw";
import { extOfFile, saveImage } from "../../ImageStore";

/**
 * 与官方 insertImageButton 同款：临时创建一个隐藏的 file input 唤起系统文件选择器，
 * 选中后逐个存进图片目录，拿到可持久访问的 URL 再插入编辑器。
 * input 在 click() 后立刻从 DOM 移除，但 change 监听由闭包持有，选完文件仍会触发。
 */
function insertImageFromFile(editor: IEditor): void {
  const doc = editor.getDocument();
  const input = doc.createElement("input");
  input.type = "file";
  input.accept = "image/*";
  input.style.display = "none";
  doc.body.appendChild(input);
  input.addEventListener("change", async () => {
    for (const file of input.files ?? []) {
      const url = await saveImage(file, extOfFile(file));
      if (url) {
        insertImage(editor, url);
      }
    }
  });
  input.click();
  doc.body.removeChild(input);
}

export const imageButton = createButton({
  icon: imageSvg,
  title: "插入图片",
  onClick: insertImageFromFile,
});
