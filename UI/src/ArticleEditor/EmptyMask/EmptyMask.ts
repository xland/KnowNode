import "./EmptyMask.scss";
import html from "./EmptyMask.html?raw";
import CtrlBase from "../../CtrlBase";
import Msg from "../../Msg";

/**
 * 右侧编辑面板的"没有文章"遮罩（模块单例）。
 * 由 ArticleTitle 在列表重画完之后按"有没有行"来开合：列表被过滤空了（选中了空分类、
 * 或整个库一篇都没有的首次启动）、以及删掉最后一篇之后，就盖住整个编辑面板，中间只留一个
 * 「新建文章」按钮。
 * 按钮只广播 addArticle，跟 Header 上的加号走同一条路——真正建库、插到列表最前面并选中它
 * 的动作在 ArticleTitle 里（没选中分类就是未分类，选中了就建在该分类下）。
 */
class EmptyMask extends CtrlBase {
  /** 挂到面板之前就收到过的显隐指令：ready() 时补上（ArticleTitle 的首次载入是异步回来的） */
  private pending = false;

  constructor() {
    super(html);
  }

  override ready(): void {
    this.dom.querySelector<HTMLElement>("#emptyMaskBtn").addEventListener("click", () => Msg.emit("addArticle"));
    this.setVisible(this.pending);
  }

  /** 盖住 / 收起遮罩 */
  setVisible(visible: boolean): void {
    if (!this.dom) {
      // 还没挂上：记下来，ready() 里再应用
      this.pending = visible;
      return;
    }
    this.dom.style.display = visible ? "flex" : "none";
  }
}

export default new EmptyMask();
