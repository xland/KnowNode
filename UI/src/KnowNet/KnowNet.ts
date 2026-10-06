import "./KnowNet.scss";
import html from "./KnowNet.html?raw";
import CtrlBase from "../CtrlBase";
import Msg from "../Msg";
import Konva from "konva";
import KnowDetail from "../KnowDetail/KnowDetail";
import StatusBar from "../StatusBar/StatusBar";

interface NodeData {
  id: number;
  title: string;
  x: number;
  y: number;
}

interface LineData {
  id: number;
  nodeAId: number;
  nodeBId: number;
}

/**
 * 节点矩形的尺寸参数（Arch/32 里标着待确认，这里先取一组能用的值，集中在一处方便调）。
 * NODE_MIN_WIDTH / NODE_MAX_WIDTH 之外的部分靠文字省略号处理。
 */
const NODE_HEIGHT = 36;
const NODE_MIN_WIDTH = 72;
const NODE_MAX_WIDTH = 220;
const NODE_PADDING = 24; // 文字两侧留白之和，用来按文字长度算宽度
const NODE_FONT_SIZE = 13;
const NODE_RADIUS = 6;

/** 缩放范围与步长（上下限同样待确认，先取常用的一组） */
const SCALE_MIN = 0.2;
const SCALE_MAX = 3;
const SCALE_STEP = 1.08;

/** 新建节点的默认标题（与库里的默认值、KnowDetail 标题框的回落值保持一致，见 Arch/33） */
const DEFAULT_TITLE = "未命名";
/** 新建关联节点与源节点的水平间距（Arch/32 取 60px），y 与源节点对齐 */
const NEW_NODE_GAP = 60;
/** 「+」按钮：中心离节点右边缘这么远，半径固定 */
const ADD_BUTTON_OFFSET = 15;
const ADD_BUTTON_RADIUS = 9;

/**
 * 中间的知识节点网络画布（模块单例，见 Arch/32），用 Konva 绘制。
 *
 * 分工：**布局坐标是数据、Konva 只负责按坐标画**。节点的 x/y 由用户拖拽决定并写回库（node.move），
 * 缩放平移改的是 stage 的变换，不动节点坐标。
 *
 * 坐标约定：节点的 (x, y) 是**矩形中心**。这样新建知识的第一个节点取 (0, 0)、
 * 打开知识时把视口中心对到 (0, 0)，"画布中心 = 视口中心"就自然成立（见 Arch/32）。
 *
 * 已做到：加载并渲染节点与贝塞尔连线、拖拽移动并入库、单击选中（通知 KnowDetail 与 StatusBar）、
 * 空白处点击取消选中、滚轮缩放、空白处拖拽平移、右上角「展示全部」、
 * 双击就地改标题、选中节点右侧「+」按钮与 Tab 新建关联节点。
 * 拖拽连线、右键菜单、点连线打开详情、一键整理布局见下一版。
 */
class KnowNet extends CtrlBase {
  private stage: Konva.Stage | null = null;
  private lineLayer: Konva.Layer | null = null;
  private nodeLayer: Konva.Layer | null = null;

  private nodes: NodeData[] = [];
  private lines: LineData[] = [];
  /** 节点 id → 它的 Group，拖拽落库与选中换样式都要按 id 反查 */
  private groups = new Map<number, Konva.Group>();

  private listId: number | null = null;
  private selectedNodeId: number | null = null;
  /** 正在就地改标题的节点 id（非 null 时画布不平移，见 beginTitleEdit） */
  private editingNodeId: number | null = null;

  constructor() {
    super(html);
  }

  override ready(): void {
    const container = this.dom.querySelector<HTMLDivElement>(".knowNetCanvas")!;
    this.stage = new Konva.Stage({ container, width: container.clientWidth, height: container.clientHeight });
    this.lineLayer = new Konva.Layer();
    this.nodeLayer = new Konva.Layer();
    this.stage.add(this.lineLayer, this.nodeLayer);
    // 空白处拖拽 = 平移画布（拖节点时命中的是节点，不会平移）
    this.stage.draggable(true);

    // 面板宽度会被 splitter 拖动、窗口也会被拉伸：跟着容器改 stage 尺寸，否则画布不跟着变
    new ResizeObserver(() => this.syncSize()).observe(container);

    // 滚轮缩放：以鼠标为锚点，光标下的那个点缩放前后停在同一处
    this.stage.on("wheel", (e) => {
      e.evt.preventDefault();
      // 正在改标题时滚了滚轮：先收掉编辑，输入框不会跟着画布一起缩放平移
      if (this.editingNodeId != null) this.endTitleEdit(true);
      this.zoomBy(e.evt.deltaY < 0 ? SCALE_STEP : 1 / SCALE_STEP, this.stage!.getPointerPosition());
    });

    // 点空白处（事件的 target 就是 stage 本身）= 取消选中，KnowDetail 随之关掉
    this.stage.on("click", (e) => {
      if (e.target === this.stage) this.select(null);
    });

    this.dom.querySelector<HTMLElement>("#knowNetFit")!.addEventListener("click", () => this.fit());

    // 标题输入框：回车 / 失焦 = 提交，Esc = 放弃。按键不外传，免得画布的 Tab 快捷键收到
    this.titleEditor.addEventListener("keydown", (e) => {
      e.stopPropagation();
      if (e.key === "Enter") this.endTitleEdit(true);
      else if (e.key === "Escape") this.endTitleEdit(false);
    });
    this.titleEditor.addEventListener("blur", () => this.endTitleEdit(true));

    // Tab = 以当前选中的节点为起点新建一个关联节点
    document.addEventListener("keydown", this.onKeyDown);
  }

  /** 加载某个知识的节点与连线并画出来（点列表项时由 KnowList 调用） */
  async open(listId: number): Promise<void> {
    this.endTitleEdit(true); // 换一张网之前把正在改的标题落库
    this.listId = listId;
    this.select(null);
    try {
      const data = (await Msg.invoke("list.open", { id: listId })) as { nodes?: NodeData[]; lines?: LineData[] };
      // 请求是异步的：回来时用户可能已经点了别的知识，过期结果直接丢掉
      if (listId !== this.listId) return;
      this.nodes = data?.nodes ?? [];
      this.lines = data?.lines ?? [];
    } catch {
      this.nodes = [];
      this.lines = [];
    }
    this.render();
    // 打开一张网时视口居中到画布原点：第一个节点就落在视口中心
    this.centerOrigin();
    StatusBar.setCount(this.nodes.length);
  }

  /**
   * KnowDetail 里改了标题之后同步画布上的文字（标题是同一份数据，两处都显示它）。
   * 只改文字与宽度，不动坐标，也不用重画整张图。
   */
  updateNodeTitle(id: number, title: string): void {
    const node = this.nodes.find((n) => n.id === id);
    if (!node) return;
    node.title = title;
    const group = this.groups.get(id);
    if (!group) return;
    const text = group.findOne<Konva.Text>("Text");
    if (!text) return;
    text.text(title);
    // 宽度跟着文字长度变：夹在上下限之间，矩形与文字一起改
    const measure = new Konva.Text({ text: title, fontSize: NODE_FONT_SIZE });
    const width = Math.min(NODE_MAX_WIDTH, Math.max(NODE_MIN_WIDTH, measure.width() + NODE_PADDING));
    const rect = group.findOne<Konva.Rect>("Rect");
    rect?.x(-width / 2);
    rect?.width(width);
    text.width(width);
    text.offsetX(width / 2);
    this.redrawLines();
  }

  /** 清空画布（删掉当前打开的知识、或没有任何知识时） */
  clear(): void {
    this.endTitleEdit(true);
    this.listId = null;
    this.nodes = [];
    this.lines = [];
    this.select(null);
    this.render();
    StatusBar.setCount(0);
  }

  private syncSize(): void {
    if (!this.stage) return;
    const container = this.stage.container();
    this.stage.width(container.clientWidth);
    this.stage.height(container.clientHeight);
  }

  private render(): void {
    this.lineLayer?.destroyChildren();
    this.nodeLayer?.destroyChildren();
    this.groups.clear();

    for (const line of this.lines) {
      const a = this.nodes.find((n) => n.id === line.nodeAId);
      const b = this.nodes.find((n) => n.id === line.nodeBId);
      if (!a || !b) continue;
      this.lineLayer?.add(this.buildLine(a, b));
    }
    for (const node of this.nodes) {
      const group = this.buildNode(node);
      this.groups.set(node.id, group);
      this.nodeLayer?.add(group);
    }
  }

  /** 连线：两点之间的三次贝塞尔，控制点按两节点的相对位置取（水平方向拉开，曲线更柔和） */
  private buildLine(a: NodeData, b: NodeData): Konva.Path {
    const dx = b.x - a.x;
    const offset = Math.max(40, Math.abs(dx) * 0.5);
    const c1x = a.x + (dx >= 0 ? offset : -offset);
    const c2x = b.x - (dx >= 0 ? offset : -offset);
    return new Konva.Path({
      data: `M ${a.x} ${a.y} C ${c1x} ${a.y}, ${c2x} ${b.y}, ${b.x} ${b.y}`,
      stroke: "#b6bac1",
      strokeWidth: 1.5,
      lineCap: "round",
      // 连线不吃鼠标事件：这一版还没做"点连线打开它的详情"
      listening: false,
    });
  }

  /**
   * 一个节点 = 圆角矩形 + 居中标题。宽度按标题长度自适应，夹在 [NODE_MIN_WIDTH, NODE_MAX_WIDTH]，
   * 文字超出用省略号（悬停/选中看全文由 title 与显示层保证，见 Arch/33）。
   */
  private buildNode(node: NodeData): Konva.Group {
    const measure = new Konva.Text({ text: node.title, fontSize: NODE_FONT_SIZE });
    const width = Math.min(NODE_MAX_WIDTH, Math.max(NODE_MIN_WIDTH, measure.width() + NODE_PADDING));
    const group = new Konva.Group({ x: node.x, y: node.y, draggable: true });

    const rect = new Konva.Rect({
      x: -width / 2,
      y: -NODE_HEIGHT / 2,
      width,
      height: NODE_HEIGHT,
      fill: "#ffffff",
      stroke: "#d9dde3",
      strokeWidth: 1,
      cornerRadius: NODE_RADIUS,
      shadowColor: "#000",
      shadowBlur: 2,
      shadowOpacity: 0.08,
      shadowOffsetY: 1,
    });
    const text = new Konva.Text({
      text: node.title,
      fontSize: NODE_FONT_SIZE,
      fill: "#1f2329",
      width,
      offsetX: width / 2,
      y: -7,
      align: "center",
      ellipsis: true,
      wrap: "none",
      listening: false,
    });
    group.add(rect, text, this.buildAddButton(width));

    group.on("click", () => this.select(node.id));
    // 双击 = 就地改标题：在这块覆盖一个 DOM 输入框（Konva 里画不出能打字的东西，
    // 而且 DOM 输入框才能吃到中文输入法的候选过程）
    group.on("dblclick", () => this.beginTitleEdit(node.id));
    // 拖拽结束才写库：拖动过程中每次移动都写一次没有必要，也没有意义
    group.on("dragend", () => {
      node.x = group.x();
      node.y = group.y();
      void Msg.invoke("node.move", { id: node.id, x: node.x, y: node.y });
      this.redrawLines();
    });
    return group;
  }

  /** 节点挪了位置，连线要跟着重画（连线不随节点走，它们是按坐标算出来的） */
  private redrawLines(): void {
    this.lineLayer?.destroyChildren();
    for (const line of this.lines) {
      const a = this.nodes.find((n) => n.id === line.nodeAId);
      const b = this.nodes.find((n) => n.id === line.nodeBId);
      if (!a || !b) continue;
      this.lineLayer?.add(this.buildLine(a, b));
    }
  }

  /** 选中某个节点（传 null = 取消选中）：换高亮样式，并把它的详情交给 KnowDetail */
  private select(id: number | null): void {
    if (this.selectedNodeId === id) return;
    const previous = this.selectedNodeId;
    if (previous != null) this.setHighlight(previous, false);
    this.selectedNodeId = id;
    if (id != null) this.setHighlight(id, true);

    if (id == null) {
      KnowDetail.close();
      StatusBar.setSelection("");
      return;
    }
    const node = this.nodes.find((n) => n.id === id);
    StatusBar.setSelection(node?.title ?? "");
    void KnowDetail.showNode(id, node?.title ?? "");
  }

  private setHighlight(id: number, on: boolean): void {
    const group = this.groups.get(id);
    const rect = group?.findOne<Konva.Rect>("Rect");
    if (!group || !rect) return;
    rect.stroke(on ? "#1677ff" : "#d9dde3");
    rect.strokeWidth(on ? 2 : 1);
    // 「+」按钮只在选中时出现：点它在选中节点右侧新建一个关联节点
    group.findOne<Konva.Group>(".addBtn")?.visible(on);
  }

  /** 选中节点右侧的「+」按钮：点它新建一个与它关联的节点（只在选中时显示） */
  private buildAddButton(width: number): Konva.Group {
    const btn = new Konva.Group({ name: "addBtn", x: width / 2 + ADD_BUTTON_OFFSET, y: 0, visible: false });
    btn.add(
      new Konva.Circle({ radius: ADD_BUTTON_RADIUS, fill: "#1677ff" }),
      // 加号：一横一竖，两条线都不吃事件（点击判定交给外层的圆）
      new Konva.Line({ points: [-4, 0, 4, 0], stroke: "#fff", strokeWidth: 1.6, lineCap: "round", listening: false }),
      new Konva.Line({ points: [0, -4, 0, 4], stroke: "#fff", strokeWidth: 1.6, lineCap: "round", listening: false }),
    );
    btn.on("click", (e) => {
      // 别冒泡到节点上再走一次选中流程（会把详情面板重新加载一遍）
      e.cancelBubble = true;
      if (this.selectedNodeId != null) void this.createRelatedNode(this.selectedNodeId);
    });
    btn.on("mouseenter", () => this.setCursor("pointer"));
    btn.on("mouseleave", () => this.setCursor(""));
    return btn;
  }

  private setCursor(cursor: string): void {
    const container = this.stage?.container();
    if (container) container.style.cursor = cursor;
  }

  /**
   * 新建一个与 sourceId 关联的节点：位置在它右侧 NEW_NODE_GAP 处、y 与它对齐；
   * 建完立刻建一条连线——「关联节点」= 节点 + 连线一起有（Arch/32）。
   * 新节点建好后选中它并直接进入标题编辑，用户可以马上敲名字。
   */
  private async createRelatedNode(sourceId: number): Promise<void> {
    const source = this.nodes.find((n) => n.id === sourceId);
    if (!source || this.listId == null) return;
    const rect = this.groups.get(sourceId)?.findOne<Konva.Rect>("Rect");
    const sourceWidth = rect?.width() ?? NODE_MIN_WIDTH;
    const x = source.x + sourceWidth / 2 + NODE_MIN_WIDTH / 2 + NEW_NODE_GAP;
    const y = source.y;

    let nodeId = 0;
    try {
      nodeId = (await Msg.invoke("node.create", { listId: this.listId, x, y })) as number;
    } catch {
      return; // 节点都没建出来：画布保持原样
    }
    this.nodes.push({ id: nodeId, title: DEFAULT_TITLE, x, y });
    this.render();
    StatusBar.setCount(this.nodes.length);
    this.select(nodeId);
    this.beginTitleEdit(nodeId);

    // 连线另外建：建不出来也留着这个节点（它已经入库了），用户可以再拖一次
    try {
      const lineId = (await Msg.invoke("line.create", { nodeAId: sourceId, nodeBId: nodeId })) as number;
      if (lineId) {
        this.lines.push({ id: lineId, nodeAId: sourceId, nodeBId: nodeId });
        this.redrawLines();
      }
    } catch {
      // 同上：不打断用户
    }
  }

  /**
   * 双击节点 → 在它上面盖一个 DOM 输入框就地改标题。
   * 用 DOM 而不是在 Konva 里画：Konva 没有输入控件，而且只有真的 input 才吃得到中文输入法的候选过程。
   * 输入框按当前缩放算出位置与大小；编辑期间先关掉画布平移，免得它与节点错位。
   */
  private beginTitleEdit(id: number): void {
    const node = this.nodes.find((n) => n.id === id);
    const group = this.groups.get(id);
    if (!node || !group || !this.stage) return;
    const scale = this.stage.scaleX();
    const rect = group.findOne<Konva.Rect>("Rect");
    const width = (rect?.width() ?? NODE_MIN_WIDTH) * scale;
    const height = NODE_HEIGHT * scale;
    // 节点中心在容器里的位置（含 stage 的缩放与平移）
    const pos = group.getAbsolutePosition();

    const input = this.titleEditor;
    input.value = node.title;
    input.style.display = "";
    input.style.left = `${pos.x - width / 2}px`;
    input.style.top = `${pos.y - height / 2}px`;
    input.style.width = `${width}px`;
    input.style.height = `${height}px`;
    input.style.fontSize = `${NODE_FONT_SIZE * scale}px`;
    this.editingNodeId = id;
    this.stage.draggable(false);
    input.focus();
    input.select();
  }

  /** 收掉输入框：commit = 把当前内容写回库（空串回落到「未命名」），false = 放弃这次编辑 */
  private endTitleEdit(commit: boolean): void {
    const id = this.editingNodeId;
    if (id == null) return;
    this.editingNodeId = null;
    const value = this.titleEditor.value.trim();
    this.titleEditor.style.display = "none";
    this.stage?.draggable(true);
    if (commit) void this.commitTitle(id, value || DEFAULT_TITLE);
  }

  private async commitTitle(id: number, title: string): Promise<void> {
    const node = this.nodes.find((n) => n.id === id);
    if (!node || node.title === title) return;
    try {
      await Msg.invoke("node.update", { id, title });
      this.updateNodeTitle(id, title);
      // 详情面板正开着这个节点时，把它标题框里的字也换掉（同一份数据的两个入口）
      KnowDetail.syncTitle(id, title);
      StatusBar.setSelection(title);
    } catch {
      // 写失败：画布上的标题保持原样，下一次改动还会再写一次
    }
  }

  /** Tab = 新建关联节点；焦点在输入控件里时不抢这个键（编辑器与标题框自己要用） */
  private readonly onKeyDown = (e: KeyboardEvent): void => {
    if (e.key !== "Tab") return;
    if (this.editingNodeId != null) return;
    if (this.selectedNodeId == null) return;
    const active = document.activeElement;
    if (active && active !== document.body && this.isTypingTarget(active)) return;
    e.preventDefault();
    void this.createRelatedNode(this.selectedNodeId);
  };

  /** 焦点是不是落在某个能打字的东西里（input / textarea / 富文本编辑器） */
  private isTypingTarget(el: Element): boolean {
    const tag = el.tagName;
    return tag === "INPUT" || tag === "TEXTAREA" || (el as HTMLElement).isContentEditable === true;
  }

  private get titleEditor(): HTMLInputElement {
    return this.dom.querySelector<HTMLInputElement>(".knowNetTitleEdit")!;
  }

  /** 视口中心对到画布原点：新建知识的第一个节点在 (0, 0)，于是它出现在正中 */
  private centerOrigin(): void {
    if (!this.stage) return;
    this.stage.scale({ x: 1, y: 1 });
    this.stage.position({ x: this.stage.width() / 2, y: this.stage.height() / 2 });
  }

  /**
   * 「展示全部」：按所有节点的包围盒算缩放比（留一点边距）并居中。
   * 一个节点都没有时退回 centerOrigin。
   */
  private fit(): void {
    if (!this.stage || !this.nodes.length) return void this.centerOrigin();
    const xs = this.nodes.map((n) => n.x);
    const ys = this.nodes.map((n) => n.y);
    const minX = Math.min(...xs) - NODE_MAX_WIDTH / 2;
    const maxX = Math.max(...xs) + NODE_MAX_WIDTH / 2;
    const minY = Math.min(...ys) - NODE_HEIGHT / 2;
    const maxY = Math.max(...ys) + NODE_HEIGHT / 2;
    const boxWidth = Math.max(1, maxX - minX);
    const boxHeight = Math.max(1, maxY - minY);
    const margin = 40;
    const scale = Math.min(
      (this.stage.width() - margin * 2) / boxWidth,
      (this.stage.height() - margin * 2) / boxHeight,
    );
    const clamped = Math.min(SCALE_MAX, Math.max(SCALE_MIN, scale));
    this.stage.scale({ x: clamped, y: clamped });
    // 让包围盒中心落在视口中心：scale 之后的坐标要乘上缩放比
    this.stage.position({
      x: this.stage.width() / 2 - ((minX + maxX) / 2) * clamped,
      y: this.stage.height() / 2 - ((minY + maxY) / 2) * clamped,
    });
  }

  /** 滚轮缩放：以 anchor（视口内坐标）为锚点，它下面的画布点缩放前后位置不变 */
  private zoomBy(factor: number, anchor: Konva.Vector2d | null): void {
    if (!this.stage) return;
    const oldScale = this.stage.scaleX();
    const newScale = Math.min(SCALE_MAX, Math.max(SCALE_MIN, oldScale * factor));
    if (newScale === oldScale) return;
    const point = anchor ?? { x: this.stage.width() / 2, y: this.stage.height() / 2 };
    const position = this.stage.position();
    // 锚点在画布坐标里的位置：缩放后还让它落在同一个视口位置
    const originX = (point.x - position.x) / oldScale;
    const originY = (point.y - position.y) / oldScale;
    this.stage.scale({ x: newScale, y: newScale });
    this.stage.position({ x: point.x - originX * newScale, y: point.y - originY * newScale });
  }
}

export default new KnowNet();
