import "./EditorBar.scss";
import html from "./EditorBar.html?raw";
import CtrlBase from "../CtrlBase";
import Heading from "./Heading/Heading";
import Code from "./Code/Code";
import FontFamily from "./FontFamily/FontFamily";
import FontSize from "./FontSize/FontSize";
import TextColor from "./TextColor/TextColor";
import BackgroundColor from "./BackgroundColor/BackgroundColor";
import Align from "./Align/Align";
import LineHeight from "./LineHeight/LineHeight";
import Link from "./Link/Link";
import { undoButton } from "./Undo/Undo";
import { redoButton } from "./Redo/Redo";
import { boldButton } from "./Bold/Bold";
import { italicButton } from "./Italic/Italic";
import { underlineButton } from "./Underline/Underline";
import { strikethroughButton } from "./Strikethrough/Strikethrough";
import { subscriptButton } from "./Subscript/Subscript";
import { superscriptButton } from "./Superscript/Superscript";
import { quoteButton } from "./Quote/Quote";
import { listBulletButton } from "./ListBullet/ListBullet";
import { listNumberButton } from "./ListNumber/ListNumber";
import { linkRemoveButton } from "./LinkRemove/LinkRemove";
import { imageButton } from "./Image/Image";
import { horizonLineButton } from "./HorizonLine/HorizonLine";

/**
 * 工具栏分组：组内按声明顺序排列，组间自动插入竖向分隔线。
 * 新增工具：
 * - 无弹层的按钮 → 在各自目录实现一个按钮类（如 Bold/Bold.ts，具名导出 xxxButton），放进对应分组
 * - 带下拉/弹层的控件 → 在各自目录实现 CtrlBase（弹层开合/定位/外部关闭逻辑各控件自带），放进对应分组
 */
const GROUPS: CtrlBase[][] = [
  [undoButton, redoButton],
  [
    Heading,
    FontFamily,
    FontSize,
    boldButton,
    italicButton,
    underlineButton,
    strikethroughButton,
    subscriptButton,
    superscriptButton,
  ],
  [TextColor, BackgroundColor, quoteButton],
  [Align, LineHeight],
  [listBulletButton, listNumberButton],
  // 插入类：链接 + 图片 + 代码块 + 横线
  [Link, linkRemoveButton, imageButton, Code, horizonLineButton],
];

/**
 * 编辑器工具栏（模块单例）。
 * 根元素 #editorBar 由 ArticleEditor 挂到标题栏之后。
 */
class EditorBar extends CtrlBase {
  constructor() {
    super(html);
  }

  override ready(): void {
    GROUPS.forEach((group, index) => {
      if (index > 0) {
        this.appendDivider();
      }
      for (const ctrl of group) {
        ctrl.appendTo(this.dom);
      }
    });
    // 末尾分隔线，与原有视觉一致
    this.appendDivider();
  }

  /** 追加一条纵向分隔线 */
  private appendDivider(): void {
    const div = document.createElement("div");
    div.className = "divider";
    this.dom.appendChild(div);
  }
}

export default new EditorBar();
