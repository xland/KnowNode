import { IMAGE_URL_PREFIX, originNameOf, resizeImage } from "../ImageStore";
import type { IEditor } from "roosterjs-content-model-types";
import { walk } from "./ImagePlugin";

/**
 * 图片被拖着改过尺寸之后，让原生按新的显示尺寸另存一份图，正文里改成引用那一份。
 *
 * 为什么要另存一份：正文里原来引用的是原图（比如 1200px 宽），拖到 600px 只是改了显示尺寸，
 * 发布出去的仍是 1200px 那张。另存之后，发出去的就是 600px 的图。
 * 原图一直留着，且新文件名里认得出原图是谁（见 ImageStore 的 originNameOf），
 * 所以把图缩小之后再放大，是从原图重新生成的——不会拿上一张缩略图去放大，也就不失真。
 *
 * 检测手段是观察 img 的样式与宽高属性：拖拽过程中这些值每帧都在变，所以排一个防抖，
 * 停手之后才动手，一次拖拽只生成一份图。原生处理不了的格式（动图之类）拿到空文件名，
 * 就当没这回事，正文继续引用原图——和没有这个功能时一模一样。
 */
export default class ImageResizePlugin {
  private editor!: IEditor;
  private observer: MutationObserver | null = null;
  private timer = 0;
  /** 每个元素已经处理过的尺寸（"宽x高"）：换 src 会再触发一轮观察，别对同一个尺寸反复动手 */
  private done = new WeakMap<HTMLImageElement, string>();

  getName(): string {
    return "ImageResize";
  }

  initialize(editor: IEditor): void {
    this.editor = editor;
    this.observer = new MutationObserver(this.schedule);
    // 只看属性：正文中打字改的是节点，不会惊动这里；只看这几个属性，免得每个字符都来一趟
    this.observer.observe(editor.getDocument(), {
      subtree: true,
      attributes: true,
      attributeFilter: ["style", "width", "height", "src"],
    });
  }

  dispose(): void {
    this.observer?.disconnect();
    this.observer = null;
    this.clearTimer();
  }

  /** 防抖：拖拽期间样式一直在变，等停手了再看 */
  private schedule = (): void => {
    this.clearTimer();
    this.timer = this.editor.getDocument().defaultView!.setTimeout(this.flush, 400);
  };

  private clearTimer(): void {
    if (this.timer) {
      this.editor.getDocument().defaultView?.clearTimeout(this.timer);
      this.timer = 0;
    }
  }

  /** 扫一遍正文里的图片，尺寸与文件对不上的就另存一份 */
  private flush = async (): Promise<void> => {
    this.timer = 0;
    if (this.editor.isDisposed()) return;
    const imgs = this.editor.getDOMHelper().queryElements("img") as HTMLImageElement[];
    for (const img of imgs) {
      const size = this.targetSize(img);
      if (!size) continue;
      const [width, height] = size;
      const key = `${width}x${height}`;
      if (this.done.get(img) === key) continue;
      const src = img.getAttribute("src") ?? "";
      if (!src.startsWith(IMAGE_URL_PREFIX)) continue; // 外链图、还没落盘的图：都不动
      // file 是正文里当前引用的那份：原生拿它把库里的记录改指到新的一份（记录比正文滞后一步）
      const file = src.slice(IMAGE_URL_PREFIX.length);
      // 原生另存出新的一份之后，会把这张原图早先拖出来的其它尺寸一并清掉，
      // 目录里因此只剩原图与最后拖出来的那一份（见 ImageStore 的 resizeImage）
      const name = await resizeImage(originNameOf(file), width, height, file);
      // 成了没成都记下来：失败的（格式不支持）别再来第二轮，尺寸真改了才会重试
      this.done.set(img, key);
      if (!name) continue;
      this.replaceSrc(src, IMAGE_URL_PREFIX + name);
    }
  };

  /**
   * 图显示出来的尺寸：拖过之后 roosterjs 写在 style 上，没拖过的没有这个尺寸。
   * 与文件本身的像素尺寸（naturalWidth/Height）一样大就不用管——这一张本来就是原尺寸
   */
  private targetSize(img: HTMLImageElement): [number, number] | null {
    const width = parseFloat(img.style.width || img.getAttribute("width") || "");
    const height = parseFloat(img.style.height || img.getAttribute("height") || "");
    // 没显示尺寸（没拖过），或图还没加载出来（拿不到文件尺寸，无从比较）：都不动
    if (!width || !height || !img.naturalWidth || !img.naturalHeight) return null;
    if (Math.abs(width - img.naturalWidth) <= 1 && Math.abs(height - img.naturalHeight) <= 1) return null;
    return [Math.round(width), Math.round(height)];
  }

  /**
   * 改模型而不是直接改 DOM：roosterjs 缓存着一份内容模型，只改 img.src 的话缓存里仍是旧地址，
   * 正文入库时存的还是旧的那张
   */
  private replaceSrc(from: string, to: string): void {
    if (this.editor.isDisposed()) return;
    this.editor.formatContentModel(
      (model) => {
        let changed = false;
        walk(model, (segment) => {
          if (segment.segmentType === "Image" && segment.src === from) {
            segment.src = to;
            changed = true;
          }
        });
        return changed;
      },
      { apiName: "replaceImageSrc", skipDOMSelection: true },
    );
  }
}
