import "./Menu.scss";
import html from "./Menu.html?raw";
import CtrlBase from "../../CtrlBase";

/**
 * 文章列表行的右键菜单（模块单例）。
 * 平时不在 DOM 里：第一次 open() 才把模板挂到 document.body
 * （不能挂在 #articleTitle 里，列表所在的 .articleListContent 是 overflow-y:auto
 * 的滚动容器，弹层放在里面会被它裁掉），之后只切换 .open。
 * 只有一个菜单项"删除此文章"：删掉哪一篇由调用方（ArticleTitle）按右键命中的那一行决定，
 * 这里只负责把这一行原样回传。
 */
class Menu extends CtrlBase {
  /** 被右键的列表行（.articleItem） */
  private item: HTMLElement | null = null;

  /** 菜单项被点击时的回调，由 ArticleTitle 注册；带上被右键的那一行 */
  onRemove: ((item: HTMLElement) => void) | null = null;

  /** 窗口失焦收起：定义成类字段而不是行内箭头函数，后者每次都是新引用，想解绑也解不掉 */
  private onWindowBlur = (): void => this.close();

  constructor() {
    super(html);
  }

  override ready(): void {
    this.dom.addEventListener("click", (e) => {
      if (!(e.target as HTMLElement).closest(".menuItem")) return;
      const item = this.item;
      // 先收菜单再回调：回调里会重新加载列表，菜单不该还盖在上面
      this.close();
      this.onRemove?.(item);
    });

    // 点空白、页面滚动、窗口失焦、Esc 都关闭
    document.addEventListener("mousedown", (e) => {
      if (!this.dom?.contains(e.target as Node)) this.close();
    });
    // 用捕获：滚动可能发生在列表这个内层滚动容器上，scroll 不冒泡
    document.addEventListener("scroll", () => this.close(), true);
    document.addEventListener("keydown", (e) => {
      if (e.key === "Escape") this.close();
    });
    window.addEventListener("blur", this.onWindowBlur);
  }

  /** 在视口坐标 (x, y) 弹出菜单，item 是被右键的列表行 */
  open(x: number, y: number, item: HTMLElement): void {
    this.item = item;
    if (!this.dom) {
      this.appendTo(document.body);
    }
    // 先显示再量尺寸：隐藏时量到的是 0，越界回收的判断会失效
    this.dom.classList.add("open");
    const { width, height } = this.dom.getBoundingClientRect();
    // 贴住鼠标位置，超出窗口就往回收，四周留 4px
    this.dom.style.left = `${Math.max(4, Math.min(x, window.innerWidth - width - 4))}px`;
    this.dom.style.top = `${Math.max(4, Math.min(y, window.innerHeight - height - 4))}px`;
  }

  close(): void {
    this.dom?.classList.remove("open");
  }
}

export default new Menu();
