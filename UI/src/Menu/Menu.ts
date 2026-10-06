import "./Menu.scss";
import html from "./Menu.html?raw";
import CtrlBase from "../CtrlBase";

export interface MenuItem {
  /** 菜单项文字 */
  label: string;
  /** 点这一项要做的事；菜单在它执行前就已关掉 */
  onSelect: () => void;
}

/**
 * 通用右键菜单（DOM 实现），供 KnowList（重命名/删除知识）与 KnowNet（新建/删除节点、删除连线）共用。
 *
 * 挂到 body 上：面板大多有 overflow: hidden，菜单留在面板里会被裁掉。
 * 关闭时机：点某一项、点到别处、按 Esc、窗口失焦——菜单是"点一下就没了"的东西，
 * 任何一次新的 pointerdown 都该把它收掉（这里的监听挂在捕获阶段，先于各面板自己的处理）。
 */
class Menu extends CtrlBase {
  constructor() {
    super(html);
  }

  override ready(): void {
    document.addEventListener("pointerdown", (e) => {
      // 点在自己身上不算：那是选某一项，由项自己的 click 处理
      if (this.dom.classList.contains("show") && !this.dom.contains(e.target as Node)) this.close();
    }, true);
    window.addEventListener("blur", () => this.close());
    document.addEventListener("keydown", (e) => {
      if (e.key === "Escape") this.close();
    });
  }

  /** 在屏幕坐标 (x, y) 处弹出菜单；再次调用会先关掉上一处 */
  open(x: number, y: number, items: MenuItem[]): void {
    this.dom.replaceChildren();
    for (const item of items) {
      const el = document.createElement("div");
      el.className = "menuItem";
      el.textContent = item.label;
      el.addEventListener("click", () => {
        this.close();
        item.onSelect();
      });
      this.dom.appendChild(el);
    }
    this.dom.classList.add("show");

    // 先显示再按实际尺寸夹住位置：贴近视口右/下边缘时不能被裁掉
    const rect = this.dom.getBoundingClientRect();
    const left = Math.min(x, window.innerWidth - rect.width - 2);
    const top = Math.min(y, window.innerHeight - rect.height - 2);
    this.dom.style.left = `${Math.max(2, left)}px`;
    this.dom.style.top = `${Math.max(2, top)}px`;
  }

  close(): void {
    this.dom.classList.remove("show");
  }
}

export default new Menu();
