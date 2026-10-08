import "./KnowList.scss";
import html from "./KnowList.html?raw";
import CtrlBase from "../CtrlBase";
import Msg from "../Msg";
import Dialog from "../Dialog/Dialog";
import Menu from "../Menu/Menu";
import KnowNet from "../KnowNet/KnowNet";

/** 原生侧返回的知识行 */
interface KnowRow {
  id: number;
  name: string;
}

/**
 * 知识名称的字符上限。
 * 与节点标题取同一个值（见 Arch/33 的 36 字符规则，Arch/34 里这一条仍标着待确认）：
 * 按 Unicode 码点计数，输入框的 maxLength 本身就是按码点算的，不用另写校验。
 */
const NAME_MAX_LENGTH = 36;

/**
 * 左侧知识列表面板（模块单例，见 Arch/34）。
 *
 * 面板结构：顶部一条标题栏（左「知识列表」、右新建按钮）+ 下方列表（一项 = 一个"知识"）。
 * 点某一项 = 选中它并让 KnowNet 加载这张网（列表与画布是「选哪个知识 → 显示哪张网」的关系）。
 * 列表项右键 → 修改知识名称 / 删除知识；新建走右上角加号 → 悬浮对话框（Dialog）。
 *
 * 知识是"一张网的入口"：新建知识时原生侧（`list.create`）**只插 know_list 这一行**
 * （外加它自己的那份详情），**不建任何节点**——所以建完直接选中它，但 `KnowNet` 加载出来
 * 是**空画布**，第一个节点要用户在空白处右键新建。
 * 「未命名」是**新建节点的默认标题**，不是"新建知识时自动送的那个节点"。
 */
class KnowList extends CtrlBase {
  /** 列表容器 */
  private list: HTMLElement | null = null;

  /** 当前选中的行与它的 id */
  private selectedItem: HTMLElement | null = null;
  private selectedId: number | null = null;

  constructor() {
    super(html);
  }

  override ready(): void {
    this.list = this.dom.querySelector<HTMLElement>(".knowListContent")!;
    const addBtn = this.dom.querySelector<HTMLElement>("#knowListAdd")!;
    addBtn.addEventListener("click", () => {
      // 浮层正贴在这个按钮下面：再点一次 = 收起
      // （Dialog 的"点外面就收"故意放过了锚点本身，交给这里判断，免得收了又立刻弹开）
      if (Dialog.isOpen()) {
        Dialog.dismiss();
        return;
      }
      void this.create(addBtn);
    });
    // 整个列表只挂一个右键监听，命中哪一行事后按 e.target 反查
    this.list.addEventListener("contextmenu", (e) => this.onContextMenu(e));
    void this.loadAndRender();
  }

  /** 当前选中的知识 id；没有选中时是 null */
  get currentId(): number | null {
    return this.selectedId;
  }

  /** 重新拉一次列表并重画（新建/删除之后用） */
  async loadAndRender(): Promise<void> {
    let rows: KnowRow[] = [];
    try {
      rows = ((await Msg.invoke("list.list")) as KnowRow[]) ?? [];
    } catch {
      rows = []; // 读不出来就当空列表，用户还能新建
    }
    this.render(rows);
  }

  private render(rows: KnowRow[]): void {
    if (!this.list) return;
    this.list.replaceChildren();
    for (const row of rows) {
      this.list.appendChild(this.buildRow(row));
    }
    // 旧行连同选中样式都被丢了：selectedItem 指向的是不在文档里的节点
    this.selectedItem = null;
  }

  /** 一行 = 知识名称（过长省略） */
  private buildRow(row: KnowRow): HTMLElement {
    const item = document.createElement("div");
    item.className = "knowItem";
    item.dataset.id = String(row.id);
    item.textContent = row.name;
    item.title = row.name;
    item.addEventListener("click", () => void this.select(item));
    return item;
  }

  /** 点一行：戴上选中样式，并让画布加载这个知识的节点 */
  private async select(item: HTMLElement): Promise<void> {
    const id = Number(item.dataset.id);
    this.selectedItem?.classList.remove("selected");
    this.selectedItem = item;
    this.selectedId = id;
    item.classList.add("selected");
    await KnowNet.open(id);
  }

  /**
   * 右上角加号：在它下面浮出一个小输入框（不要遮罩，见 Dialog 的锚定形态），
   * 填好名字建库，然后插到列表末尾并选中它。
   */
  private async create(anchor: HTMLElement): Promise<void> {
    const name = await Dialog.prompt({
      anchor,
      placeholder: "知识名称",
      maxLength: NAME_MAX_LENGTH,
    });
    if (!name) return; // 取消或空名字：什么都不做
    try {
      const id = (await Msg.invoke("list.create", { name })) as number;
      const item = this.buildRow({ id, name });
      this.list?.appendChild(item);
      await this.select(item);
    } catch {
      // 入库失败：列表保持原样，用户可以再点一次加号
    }
  }

  /** 右键列表：命不到行就交给默认菜单；命到行就先选中它再弹菜单 */
  private onContextMenu(e: MouseEvent): void {
    const item = (e.target as HTMLElement).closest<HTMLElement>(".knowItem");
    if (!item) return;
    e.preventDefault();
    void this.select(item);
    const id = Number(item.dataset.id);
    Menu.open(e.clientX, e.clientY, [
      { label: "修改知识名称", onSelect: () => void this.rename(id, item) },
      { label: "删除知识", onSelect: () => void this.remove(id, item) },
    ]);
  }

  /** 重命名：对话框里回填旧名字，改完直接改这一行的文字（不整片重画，选中态不丢） */
  private async rename(id: number, item: HTMLElement): Promise<void> {
    const name = await Dialog.prompt({
      title: "修改知识名称",
      label: "知识名称",
      value: item.textContent ?? "",
      maxLength: NAME_MAX_LENGTH,
    });
    if (!name) return;
    try {
      await Msg.invoke("list.update", { id, name });
      item.textContent = name;
      item.title = name;
    } catch {
      // 改失败：界面保持原样
    }
  }

  /**
   * 删除知识：库里删掉（连带它的节点与详情），再从列表里摘掉这一行。
   * 删的正是当前打开的那张网时，改选中剩下的第一行；一行都不剩就把画布清空。
   * （问不问用户确认见 Arch/34 的待确认，这一版先直接删。）
   */
  private async remove(id: number, item: HTMLElement): Promise<void> {
    try {
      await Msg.invoke("list.remove", { id });
    } catch {
      return; // 没删掉：列表保持原样
    }
    const wasSelected = this.selectedId === id;
    item.remove();
    if (!wasSelected) return;
    const first = this.list?.firstElementChild as HTMLElement | null;
    if (first) await this.select(first);
    else {
      this.selectedItem = null;
      this.selectedId = null;
      KnowNet.clear();
    }
  }
}

export default new KnowList();
