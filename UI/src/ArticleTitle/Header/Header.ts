import "./Header.scss";
import html from "./Header.html?raw";
import CtrlBase from "../../CtrlBase";
import Msg from "../../Msg";

/**
 * 文章列表面板的标题栏（模块单例）。
 * 根元素 #articleListHeader 由 ArticleTitle 挂到 #articleTitle 的最前面，文章列表挂在它后面；
 * 右侧"添加文章"按钮只负责广播 addArticle，真正的动作（清空输入、建一篇【未命名】并选中）在 ArticleTitle 里。
 */
class Header extends CtrlBase {
  constructor() {
    super(html);
  }

  override ready(): void {
    this.dom.querySelector<HTMLElement>("#addArticleBtn").addEventListener("click", () => Msg.emit("addArticle"));
  }
}

export default new Header();
