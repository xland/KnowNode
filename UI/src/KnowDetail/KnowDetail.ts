import "./KnowDetail.scss";
import html from "./KnowDetail.html?raw";
import CtrlBase from "../CtrlBase";
import Msg from "../Msg";
import EditorContent from "../EditorContent/EditorContent";
import EditorBar from "../EditorBar/EditorBar";

/**
 * 详情面板当前挂在谁身上：
 * - `node` / `line`：画布上选中的节点 / 连线（二者行为一致，都只是那份富文本）；
 * - `list`：**知识本身**——没选中节点 / 连线时的落点，写的是这个知识整体的描述（2026-10-07 加）。
 */
type Target = { kind: "node" | "line" | "list"; id: number } | null;

/** 即时保存的防抖窗口：停手这么久才写库（Arch/33） */
const SAVE_DELAY = 300;

/**
 * 右侧详细内容面板（模块单例，见 Arch/33）。
 *
 * 同一个面板按点击对象换内容：点节点、点连线、点知识本身**行为完全一致**，都只是那份富文本详情
 * （2026-10-07：详情里的标题输入框已移除——改标题只在画布上双击节点改，一处入口就够）。
 * 编辑即存，不设保存按钮：改完停手 300ms 写一次库；切换对象、收面板、关窗口时把没落库的立刻 flush 掉。
 *
 * 它是**三栏布局的第三栏**（Arch/31）：与 KnowList、KnowNet 并排，拖它左边缘那条 splitter 调宽。
 *
 * **展开与否由用户说了算**（2026-10-07 改定）：选中节点 / 连线**不会**自动展开面板，
 * 用户点右侧那个按钮才展开（默认收起）。展开期间切换对象，内容跟着换；收起时先把没落的写完。
 */
class KnowDetail extends CtrlBase {
  private target: Target = null;
  /** 面板现在是展开的还是收起的（默认收起，见上） */
  private isOpen = false;

  /** 防抖定时器；0 = 没排着队 */
  private saveTimer = 0;
  /** 有待入库的改动 */
  private dirty = false;
  /** 正在飞的那一轮写库；关窗前要等它落地 */
  private savingNow: Promise<void> | null = null;
  private saving = false;
  /** 程序化回填内容（切换对象时）：这期间的 contentChanged 不是用户在编辑 */
  private suppress = false;

  constructor() {
    super(html);
  }

  override ready(): void {
    // 工具栏必须**先**挂：EditorPlugin 在 new Editor() 的那一刻就广播第一帧 editorState，
    // 按钮的启用/点亮态全靠它。挂晚了会错过这帧，撤销/重做/移除链接会一直保持初始置灰，
    // 直到用户敲一次键盘才有反应
    EditorBar.appendTo(this.dom.querySelector<HTMLElement>(".knowDetailBar")!);
    EditorContent.appendTo(this.dom.querySelector<HTMLElement>(".knowDetailEditor")!);
    // 正文改动与失焦：前者排队入库，后者立刻落库（紧接着关窗的话不能等那 300ms）
    Msg.on("editorContentChanged", this.onContentChanged);
    Msg.on("editorBlur", () => this.flushSave());

    // 面板本体上的点击不该穿透到画布（画布收到 click 会取消选中、把面板关掉）
    this.dom.addEventListener("click", (e) => e.stopPropagation());
    this.dom.addEventListener("contextmenu", (e) => e.stopPropagation());
  }

  /** 选中画布上的节点：面板内容切到它的详情（改标题请双击画布上的节点） */
  async showNode(id: number): Promise<void> {
    await this.switchTo({ kind: "node", id });
  }

  /** 选中连线：与节点完全一样 */
  async showLine(id: number): Promise<void> {
    await this.switchTo({ kind: "line", id });
  }

  /**
   * 没选中节点 / 连线时：面板内容回到**这个知识本身**的描述。
   * 传 null（没打开任何知识）就收起面板并清空。
   */
  async showList(listId: number | null): Promise<void> {
    if (listId == null) return void this.close();
    await this.switchTo({ kind: "list", id: listId });
  }

  /** 没有任何可显示的对象（没打开知识）：先把没落的改动写完，再收起面板 */
  close(): void {
    this.flushSave();
    this.target = null;
    void this.setOpen(false);
  }

  /** 展开 / 收起这一栏：右侧那个按钮点的就是它（ContentBox 转发过来） */
  toggle(): Promise<void> {
    return this.setOpen(!this.isOpen);
  }

  /** 把还没落库的改动立刻写完并等它落地（关窗前调用，飞在半路的 IPC 会被掐断） */
  async flush(): Promise<void> {
    clearTimeout(this.saveTimer);
    this.saveTimer = 0;
    if (this.savingNow) await this.savingNow;
    if (this.dirty) await (this.savingNow = this.saveNow());
    clearTimeout(this.saveTimer);
    this.saveTimer = 0;
  }

  /** 这一栏现在是展开的还是收起的（ContentBox 据此算画布要留住多少空间、按钮往哪贴） */
  get isExpanded(): boolean {
    return this.isOpen;
  }

  /** 有没有可展开的东西：没打开任何知识时既没有节点也没有知识本身可显示 */
  get canExpand(): boolean {
    return this.target != null;
  }

  /**
   * 展开 / 收起这一栏。它是三栏里的普通一栏，收起时 display:none，画布自然吃掉这部分宽度。
   * 状态要广播出去：ContentBox 据此把详情那条 splitter 与展开按钮重新贴到面板边缘。
   * 收起前先把没落的改动写完（用户可能紧接着就切走 / 关窗），展开时才去取当前对象的内容。
   */
  private async setOpen(open: boolean): Promise<void> {
    if (this.isOpen === open) return;
    this.isOpen = open;
    this.dom.classList.toggle("show", open);
    Msg.emit("knowDetailExpanded", { expanded: open });
    if (open) await this.load();
    else this.flushSave();
  }

  /** 换到另一个对象：先把上一个没落的改动落到它自己身上；面板正展开着就顺便把新内容取回来 */
  private async switchTo(target: Target): Promise<void> {
    const unchanged = this.target?.kind === target.kind && this.target?.id === target.id;
    const hadTarget = this.target != null;
    this.flushSave();
    this.target = target;
    // 可展开性变了（从"没有对象"到"有对象"，或反过来）：把手按钮的显隐要跟着更新
    if (hadTarget !== (target != null)) Msg.emit("knowDetailExpanded", { expanded: this.isOpen });
    // 同一个对象（比如重复点同一个节点）：别再取一遍，也别把用户正在敲的内容冲掉
    if (unchanged || !this.isOpen) return;
    await this.load();
  }

  /** 把当前对象的详情取回来填进编辑器；没有对象就清空 */
  private async load(): Promise<void> {
    const target = this.target;
    let content = "";
    if (target) {
      try {
        content = (await Msg.invoke("detail.get", { target: target.kind, id: target.id })) as string;
        // 请求是异步的：回来时用户可能已经点了别处，过期结果别往编辑器里塞
        if (this.target !== target) return;
      } catch {
        content = "";
      }
    }
    this.suppress = true;
    EditorContent.setContent(content ?? "");
    this.suppress = false;
  }

  private readonly onContentChanged = (): void => {
    if (this.suppress) return;
    this.scheduleSave();
  };

  private scheduleSave(): void {
    this.dirty = true;
    if (this.saveTimer || this.saving) return;
    this.saveTimer = window.setTimeout(() => {
      this.saveTimer = 0;
      void (this.savingNow = this.saveNow());
    }, SAVE_DELAY);
  }

  private flushSave(): void {
    if (!this.dirty) return;
    clearTimeout(this.saveTimer);
    this.saveTimer = 0;
    void (this.savingNow = this.saveNow());
  }

  private async saveNow(): Promise<void> {
    this.saveTimer = 0;
    const target = this.target;
    // 面板已经关了 / 没有对象（改动作废）：没有可以写回的地方
    if (!target) {
      this.dirty = false;
      return;
    }
    this.saving = true;
    this.dirty = false;
    try {
      // 只有详情要存：标题归画布管（双击节点改，改完由 KnowNet 自己写库）
      await Msg.invoke("detail.save", { target: target.kind, id: target.id, content: EditorContent.content });
    } catch {
      // 写失败：下一轮改动还会再写一次，这里不打断用户
    } finally {
      this.saving = false;
      this.savingNow = null;
      if (this.dirty) this.scheduleSave();
    }
  }
}

export default new KnowDetail();
