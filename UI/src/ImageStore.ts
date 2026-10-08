import Msg from "./Msg";

/** 正文里引用图片的主机前缀：原生把这个主机映射到了数据目录 */
const IMAGE_HOST_PREFIX = "https://app.localhost/";

/** 图片都存到数据目录的 images 子目录，正文里引用时的 URL 前缀 */
export const IMAGE_URL_PREFIX = IMAGE_HOST_PREFIX + "images/";

/** 原生给的图片目录句柄（数据目录下的 images），页面存活期间一直有效，取一次就够 */
let imageDir: FileSystemDirectoryHandle | null = null;

/**
 * 向原生要一次图片目录句柄：原生用 PostWebMessageAsJsonWithAdditionalObjects 把
 * File System Access 的目录句柄放进 additionalObjects 回过来，之后存文件全在 JS 侧完成。
 */
async function getImageDir(): Promise<FileSystemDirectoryHandle> {
  if (imageDir) return imageDir;
  const { objects } = await Msg.invokeWithObjects("image.dir");
  const dir = objects[0];
  // 鸭子类型而不是 instanceof：句柄是原生注入的，认它有没有目录句柄的方法更稳
  if (!dir || typeof dir.getFileHandle !== "function") {
    throw new Error("未取到图片目录句柄");
  }
  imageDir = dir;
  return dir;
}

/** MIME 子类型 → 扩展名：image/png → .png，image/svg+xml → .svg，认不出就按 .png 存 */
export function extFromMime(mime: string): string {
  const sub = (mime || "").split("/")[1];
  return sub ? "." + sub.toLowerCase().replace("+xml", "") : ".png";
}

/** 扩展名：优先用原文件名后缀（jfif/webp 这类 MIME 不一定有），取不到再退回 MIME 子类型 */
export function extOfFile(file: File): string {
  const dot = file.name.lastIndexOf(".");
  if (dot > 0) return file.name.slice(dot).toLowerCase();
  return extFromMime(file.type);
}

/** data: URL 里的 MIME（data:image/png;base64,... → image/png），取不到给空串 */
export function mimeOfDataUrl(url: string): string {
  return /^data:([^;,]+)/.exec(url)?.[1] ?? "";
}

/**
 * 把图片写进图片目录，返回可持久引用的 https://app.localhost/images/<文件名>。
 * 不用 URL.createObjectURL：blob: 只对当前会话有效，写进正文 HTML 入库后重开文章就是裂图。
 */
export async function saveImage(blob: Blob, ext: string): Promise<string> {
  const dir = await getImageDir();
  const name = `img_${Date.now()}_${Math.random().toString(36).slice(2)}${ext}`;
  const fileHandle = await dir.getFileHandle(name, { create: true });
  const writable = await fileHandle.createWritable();
  // 流式写而不是 write(blob)：边读边写，大图不必整个进内存再落盘。
  // preventClose 让关闭仍走下面那句显式的 close——FileSystemWritableFileStream 的 close
  // 带"提交落盘"的语义，显式留着比依赖 pipeTo 的隐式关闭更好读也更保险
  await blob.stream().pipeTo(writable, { preventClose: true });
  await writable.close();
  return IMAGE_URL_PREFIX + name;
}

/**
 * 图片没有"缩放产物"这一说：用户拖拽改的只是 img 的显示尺寸（style 上的 width/height），
 * 原生不再按新尺寸另存一份图，正文从头到尾引用落盘时的那一份原图。
 * 老项目的 image.resize 与 ImageResizePlugin 因此删除（图片只落盘一次，目录里不攒缩放件）。
 */
