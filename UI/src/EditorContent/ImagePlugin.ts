import { extFromMime, mimeOfDataUrl, saveImage } from "../ImageStore";
import type { ContentModelSegment, IEditor, PluginEvent, ReadonlyContentModelBlock } from "roosterjs-content-model-types";

/**
 * 把粘贴/拖放进来的内联图片（data: 或 blob:）存进图片目录，换成可持久引用的 https URL。
 * 这类图片本来没有本地文件：截图粘贴时剪贴板里就是 base64，直接入库正文会带着整张图的
 * base64 反复存取。落盘之后，正文里只剩一个文件名。
 *
 * 存盘是异步的，paste 流程本身是同步的，所以这里不拦粘贴内容，而是等 roosterjs 把内容
 * 插进 DOM 之后再扫一遍图片、挨个落盘、最后用模型把 src 换掉。
 */
export default class ImagePlugin {
  private editor!: IEditor;
  private timer = 0;

  getName(): string {
    return "ImageStore";
  }

  initialize(editor: IEditor): void {
    this.editor = editor;
  }

  dispose(): void {
    if (this.timer) {
      this.editor.getDocument().defaultView?.clearTimeout(this.timer);
      this.timer = 0;
    }
  }

  onPluginEvent(event: PluginEvent): void {
    // 粘贴/拖放的内容要等 roosterjs 真正插完才有 img 可扫，所以排到下一个宏任务
    if (event.eventType === "beforePaste" || event.eventType === "beforeDrop") {
      const win = this.editor.getDocument().defaultView!;
      win.clearTimeout(this.timer);
      this.timer = win.setTimeout(this.flush, 0);
    }
  }

  /** 扫一遍正文里还没落盘的内联图片，逐个存进图片目录 */
  private flush = async (): Promise<void> => {
    this.timer = 0;
    if (this.editor.isDisposed()) return;
    const imgs = this.editor.getDOMHelper().queryElements('img[src^="data:"], img[src^="blob:"]') as HTMLImageElement[];
    for (const img of imgs) {
      // 同一张图重复粘贴时 src 相同，所以按元素打标记，而不是按 src 去重
      if (img.dataset.uploading) continue;
      img.dataset.uploading = "1";
      const src = img.src;
      try {
        const blob = await (await fetch(src)).blob();
        const url = await saveImage(blob, extFromMime(blob.type || mimeOfDataUrl(src)));
        this.replaceSrc(src, url, img);
      } catch {
        // 存失败就保持原样：base64 至少还能显示，撤掉标记等下次粘贴再试
        delete img.dataset.uploading;
      }
    }
  };

  /**
   * 改模型而不是直接改 DOM：roosterjs 缓存着一份内容模型，只改 img.src 的话
   * 缓存里仍是 base64，正文入库时存的还是旧地址。返回 true 让模型写回并触发 contentChanged。
   */
  private replaceSrc(from: string, to: string, img: HTMLImageElement): void {
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
    delete img.dataset.uploading;
  }
}

/** 深度遍历内容模型里的所有段落（含表格单元格、列表、引用这类块组内的） */
export function walk(group: { blocks: readonly ReadonlyContentModelBlock[] }, visit: (segment: ContentModelSegment) => void): void {
  for (const block of group.blocks) {
    switch (block.blockType) {
      case "Paragraph":
        // 段落里的 segment 在 formatContentModel 回调中实际是浅可变的（能改 src），
        // 但 roosterjs 把它们标成了 readonly，这里转回可变类型再交给 visit
        (block.segments as unknown as ContentModelSegment[]).forEach(visit);
        break;
      case "Table":
        for (const row of block.rows) {
          for (const cell of row.cells) {
            walk(cell, visit);
          }
        }
        break;
      case "BlockGroup":
        walk(block, visit);
        break;
    }
  }
}
