import { createButton } from "../../ToolbarButton";
import { createDivider, iterateSelections, mutateBlock } from "roosterjs-content-model-dom";
import type {
  ContentModelBlock,
  ReadonlyContentModelBlockGroup,
  ShallowMutableContentModelBlock,
  ShallowMutableContentModelParagraph,
} from "roosterjs-content-model-types";
import type { IEditor } from "roosterjs-content-model-types";
import horizonLineSvg from "../icon/horizonLine.svg?raw";

/**
 * 光标落点：一个只有光标占位的空段落（横线之后要有地方接着写）。
 * 段落格式留空 = 用正文默认（字号行高都从 #editorContent 继承），
 * 不沿用插入处那一段的格式——否则在标题里插一条横线，后面那段也成了标题。
 */
function emptyParagraph(): ShallowMutableContentModelParagraph {
  return {
    blockType: "Paragraph",
    format: {},
    segments: [{ segmentType: "SelectionMarker", isSelected: true, format: {} }],
  };
}

/** 段落里有没有实质内容：只有光标占位或 <br> 的算空 */
function isEmpty(paragraph: { segments: readonly { segmentType: string; text?: string }[] }): boolean {
  return !paragraph.segments.some(
    (seg) =>
      (seg.segmentType === "Text" && !!seg.text) ||
      (seg.segmentType !== "Text" && seg.segmentType !== "SelectionMarker" && seg.segmentType !== "Br"),
  );
}

/**
 * 在光标处插入一条横线（<hr>）。
 * roosterjs 的模型里横线是 Divider 块（tagName: "hr"）：它自己有 hrProcessor（DOM → 模型）
 * 与 handleDivider（模型 → DOM），所以入库、回显、导出 HTML 都是标准 <hr>，不用自己拼 DOM。
 * 插法是：插在光标所在段落之后（那段是空的就直接换成横线，免得留下空行），
 * 再在横线后面补一个空段落并把光标放进去——横线后面没有段落，光标无处安放，也就没法接着往下写。
 */
function insertHorizonLine(editor: IEditor): void {
  editor.formatContentModel(
    (model) => {
      let inserted = false;
      iterateSelections(model, (path, _tableContext, block) => {
        // 多段被选中时只认第一个落点：一条横线只插一次
        if (inserted || block?.blockType !== "Paragraph") return;
        const parent = path[path.length - 1] as ReadonlyContentModelBlockGroup;
        if (!parent) return;
        const blocks = mutateBlock(parent).blocks;
        const index = blocks.findIndex((b) => (b as ContentModelBlock) === block);
        if (index < 0) return;
        const paragraph = mutateBlock(block);
        // 光标要挪到新段落上：原来那个选中标记先摘掉，不然两处都算选中
        for (const seg of paragraph.segments) seg.isSelected = false;
        const tail = emptyParagraph();
        const divider = createDivider("hr") as ShallowMutableContentModelBlock;
        if (isEmpty(paragraph)) blocks.splice(index, 1, divider, tail);
        else blocks.splice(index + 1, 0, divider, tail);
        inserted = true;
      });
      if (!inserted) {
        // 拿不到落点（空文档等）：直接追加到正文最后
        mutateBlock(model).blocks.push(createDivider("hr") as ShallowMutableContentModelBlock, emptyParagraph());
      }
      return true;
    },
    { apiName: "insertHorizonLine" },
  );
}

export const horizonLineButton = createButton({
  icon: horizonLineSvg,
  title: "横线",
  onClick: (editor) => insertHorizonLine(editor),
});
