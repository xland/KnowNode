import "./Menu.scss";
import html from "./Menu.html?raw";
import CtrlBase from "../CtrlBase";

/** 菜单项里的一枚颜色按钮：图标本身就是那个颜色，选中勾、未选中空圈 */
export interface MenuColorButton {
  /** 按钮的颜色（CSS 颜色值） */
  color: string;
  /** 是否选中：选中画 icon-check，未选中画 icon-uncheck */
  checked: boolean;
  /** 提示文字（悬停时显示，如「黄」） */
  title?: string;
  /** 点这个按钮要做的事；菜单在它执行前就已关掉 */
  onSelect: () => void;
}

export interface MenuItem {
  /** 菜单项文字；不给出就不画文字（一行颜色按钮那种） */
  label?: string;
  /** 点这一项要做的事；菜单在它执行前就已关掉。纯展示的项（颜色行）不给 */
  onSelect?: () => void;
  /** 一行并排的颜色按钮：给了它就画成一行按钮，不再画文字 */
  colors?: MenuColorButton[];
}

/**
 * 通用右键菜单（DOM 实现），供 KnowList（重命名/删除知识）与 KnowNet（新建/删除节点、删除连线、
 * 挑标记色）共用。
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
      if (item.colors) {
        const row = document.createElement("div");
        row.className = "menuColors";
        for (const button of item.colors) {
          const btn = document.createElement("span");
          btn.className = `icon menuColorBtn ${button.checked ? "icon-check" : "icon-uncheck"}`;
          btn.style.color = button.color;
          if (button.title) btn.title = button.title;
          btn.addEventListener("click", (e) => {
            // 别冒泡到菜单项：那是"点这一行"，会顺手把菜单关掉再触发一次空操作
            e.stopPropagation();
            this.close();
            button.onSelect();
          });
          row.appendChild(btn);
        }
        el.appendChild(row);
      } else if (item.label != null) {
        // 文字用 textContent 而不是 innerHTML：标题之类的文本可能带特殊字符
        const label = document.createElement("span");
        label.textContent = item.label;
        el.appendChild(label);
      }
      el.addEventListener("click", () => {
        if (!item.onSelect) return; // 纯展示项（颜色行）：点空白不关菜单
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
