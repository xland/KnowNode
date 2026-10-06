import "./KnowDetail.scss";
import html from "./KnowDetail.html?raw";
import CtrlBase from "../CtrlBase";
import Msg from "../Msg";
import EditorContent from "../EditorContent/EditorContent";
import EditorBar from "../EditorBar/EditorBar";
import KnowNet from "../KnowNet/KnowNet";

/** 详情面板当前挂在谁身上：节点（标题 + 详情）或连线（只有详情） */
type Target = { kind: "node" | "line"; id: number } | null;

/** 标题的字符上限，与画布节点、数据库共用同一个值（Arch/33） */
const TITLE_MAX_LENGTH = 36;
/** 即时保存的防抖窗口：停手这么久才写库（Arch/33） */
const SAVE_DELAY = 300;
/** 钉住状态在 setting 表里的 key（Arch/31） */
const PINNED_KEY = "know_detail.pinned";

/**
 * 右侧详细内容面板（模块单例，见 Arch/33）。
 *
 * 同一个面板按点击对象换内容：点节点 = 标题 + 详情；点连线 = 只有详情（权重已删除）。
 * 编辑即存，不设保存按钮：改完停手 300ms 写一次库；切换对象、关面板、关窗口时把没落库的立刻 flush 掉。
 *
 * 两种形态（Arch/31）：默认**悬浮**在 ContentBox 右侧盖住画布，点右上角图钉按钮后**钉住**，
 * ContentBox 变成三列布局。钉住状态存 setting 表，下次打开还是那样。
 */
class KnowDetail extends CtrlBase {
  private target: Target = null;

  /** 防抖定时器；0 = 没排着队 */
  private saveTimer = 0;
  /** 有待入库的改动 */
  private dirty = false;
  /** 正在飞的那一轮写库；关窗前要等它落地 */
  private savingNow: Promise<void> | null = null;
  private saving = false;
  /** 程序化回填内容（切换对象时）：这期间的 contentChanged 不是用户在编辑 */
  private suppress = false;

  private pinned = false;

  constructor() {
    super(html);
  }

  override ready(): void {
    // 工具栏必须**先**挂：EditorPlugin 在 new Editor() 的那一刻就广播第一帧 editorState，
    // 按钮的启用/点亮态全靠它。挂晚了会错过这帧，撤销/重做/移除链接会一直保持初始置灰，
    // 直到用户敲一次键盘才有反应
    EditorBar.appendTo(this.dom.querySelector<HTMLElement>(".knowDetailBar")!);
    EditorContent.appendTo(this.dom.querySelector<HTMLElement>(".knowDetailEditor")!);
    this.titleInput.addEventListener("input", () => this.onTitleInput());
    this.dom.querySelector<HTMLElement>("#knowDetailPin")!.addEventListener("click", () => void this.togglePin());
    // 正文改动与失焦：前者排队入库，后者立刻落库（紧接着关窗的话不能等那 300ms）
    Msg.on("editorContentChanged", this.onContentChanged);
    Msg.on("editorBlur", () => this.flushSave());

    // 面板本体上的点击不该穿透到画布（画布收到 click 会取消选中、把面板关掉）
    this.dom.addEventListener("click", (e) => e.stopPropagation());
    this.dom.addEventListener("contextmenu", (e) => e.stopPropagation());

    void this.loadPinned();
  }

  /** 点画布上的节点：显示它的标题 + 详情 */
  async showNode(id: number, title: string): Promise<void> {
    await this.switchTo({ kind: "node", id });
    this.titleInput.value = title;
    this.titleRow.style.display = "";
  }

  /** 点连线：只有详情，没有标题 */
  async showLine(id: number): Promise<void> {
    await this.switchTo({ kind: "line", id });
    this.titleRow.style.display = "none";
  }

  /**
   * 画布上就地改了标题之后同步这里的标题框（同一份数据的两个入口）。
   * 只有当前打开的正是这个节点才动；直接改 DOM 值，不再触发一轮入库（写库由画布那边做了）。
   */
  syncTitle(id: number, title: string): void {
    if (this.target?.kind !== "node" || this.target.id !== id) return;
    if (this.titleInput.value === title) return;
    this.titleInput.value = title;
  }

  /** 取消选中 / 关闭面板：先把没落的改动写完 */
  close(): void {
    this.flushSave();
    this.target = null;
    this.dom.classList.remove("show");
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

  /** 当前是否处于钉住态（ContentBox 据此切换三列布局） */
  get isPinned(): boolean {
    return this.pinned;
  }

  private get titleInput(): HTMLInputElement {
    return this.dom.querySelector<HTMLInputElement>("#knowDetailTitleInput")!;
  }

  private get titleRow(): HTMLElement {
    return this.dom.querySelector<HTMLElement>(".knowDetailTitleRow")!;
  }

  private async loadPinned(): Promise<void> {
    try {
      const value = (await Msg.invoke("setting.get", { key: PINNED_KEY, fallback: "0" })) as string;
      this.setPinned(value === "1");
    } catch {
      this.setPinned(false); // 读不出来就按默认：悬浮
    }
  }

  private async togglePin(): Promise<void> {
    this.setPinned(!this.pinned);
    // 写库失败无所谓：下次打开回到默认态而已，界面已经按用户点的那样变了
    await Msg.invoke("setting.set", { key: PINNED_KEY, value: this.pinned ? "1" : "0" }).catch(() => {});
  }

  private setPinned(pinned: boolean): void {
    this.pinned = pinned;
    this.dom.classList.toggle("pinned", pinned);
    this.dom.querySelector<HTMLElement>("#knowDetailPin")!.classList.toggle("on", pinned);
    // 布局切换（三列 / 悬浮）由 ContentBox 负责，它监听这个事件重排 splitter
    Msg.emit("knowDetailPinned", { pinned });
  }

  /** 换到另一个对象：先把上一个没落的改动落到它自己身上，再取新的详情 */
  private async switchTo(target: Target): Promise<void> {
    this.flushSave();
    this.target = target;
    this.dom.classList.add("show");

    let content = "";
    try {
      content = (await Msg.invoke("detail.get", { target: target!.kind, id: target!.id })) as string;
      // 请求是异步的：回来时用户可能已经点了别处，过期结果别往编辑器里塞
      if (this.target !== target) return;
    } catch {
      content = "";
    }
    this.suppress = true;
    EditorContent.setContent(content ?? "");
    this.suppress = false;
  }

  /** 标题改动：按码点截到上限（maxlength 是按 UTF-16 算的，emoji 会被算成两个），再排队入库 */
  private onTitleInput(): void {
    const clipped = [...this.titleInput.value].slice(0, TITLE_MAX_LENGTH).join("");
    if (clipped !== this.titleInput.value) this.titleInput.value = clipped;
    this.scheduleSave();
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
      if (target.kind === "node") {
        // 标题：空了就回落到「未命名」，与画布上新建节点的默认值一致（Arch/33）
        const title = this.titleInput.value.trim() || "未命名";
        await Msg.invoke("node.update", { id: target.id, title });
        if (this.target === target) KnowNet.updateNodeTitle(target.id, title);
      }
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
