import "./KnowNet.scss";
import html from "./KnowNet.html?raw";
import CtrlBase from "../CtrlBase";
import Msg from "../Msg";
import Konva from "konva";
import KnowDetail from "../KnowDetail/KnowDetail";
import StatusBar from "../StatusBar/StatusBar";
import Menu, { type MenuItem } from "../Menu/Menu";

interface NodeData {
  id: number;
  title: string;
  x: number;
  y: number;
  /** 标记色：0 = 未着色，1..6 对应 MARK_COLORS（与库里的 color 字段同一个值） */
  color?: number;
}

interface LineData {
  id: number;
  nodeAId: number;
  nodeBId: number;
  color?: number;
}

/**
 * 右键菜单里可选的六种标记色。索引 +1 就是存进库的 color（1..6），0 表示没着色。
 * 与 C++ 侧 KnowNode::maxColor / KnowLine::maxColor 必须一致，改一边要改另一边。
 */
const MARK_COLORS = ["#f5222d", "#fa8c16", "#fadb14", "#52c41a", "#1677ff", "#722ed1"];
/** 颜色在菜单里的名字（悬停提示用） */
const MARK_NAMES = ["红", "橙", "黄", "绿", "蓝", "紫"];

/** 把 #rrggbb 按 ratio 压暗（选中态用）；解析不出来就原样返回 */
function darken(hex: string, ratio: number): string {
  const m = /^#([0-9a-f]{6})$/i.exec(hex);
  if (!m) return hex;
  const value = parseInt(m[1], 16);
  const channel = (n: number) => Math.round(n * ratio);
  return `rgb(${channel((value >> 16) & 255)}, ${channel((value >> 8) & 255)}, ${channel(value & 255)})`;
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
const NODE_RADIUS = 3;
/** 节点边框粗细：选中与否都用这个值，两种状态只靠颜色区分 */
const NODE_STROKE_WIDTH = 1;

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
/** 节点边缘多宽算「往外拉线」的热区（按缩放折算，屏幕上恒定这么宽），中间剩下的部分才是拖节点 */
const EDGE_HOT_ZONE = 6;
/** 连线的粗细：选中与否都用这个值，只靠颜色区分（与节点边框同理） */
const LINE_WIDTH = 2;
/** 连线的点击热区：线很细，判定放宽才点得中。这个值与视觉粗细无关，别跟着 LINE_WIDTH 改 */
const LINE_HIT_WIDTH = 12;
/** 滚轮「一行」折多少像素：Firefox 的 deltaMode 给的是行数，不是像素 */
const WHEEL_LINE_HEIGHT = 16;
/** 搜不到节点时那行提示挂多久（毫秒） */
const TIP_DURATION = 2000;

/**
 * 中间的知识节点网络画布（模块单例，见 Arch/32），用 Konva 绘制。
 *
 * 分工：**布局坐标是数据、Konva 只负责按坐标画**。节点的 x/y 由用户拖拽决定并写回库（node.move），
 * 缩放平移改的是 stage 的变换，不动节点坐标。
 *
 * 坐标约定：节点的 (x, y) 是**矩形中心**。这样新建知识的第一个节点取 (0, 0)、
 * 打开知识时把视口中心对到 (0, 0)，"画布中心 = 视口中心"就自然成立（见 Arch/32）。
 *
 * 已做到：加载并渲染节点与连线、拖拽移动并入库、单击选中（通知 KnowDetail 与 StatusBar）、
 * 空白处点击取消选中、滚轮缩放、空白处拖拽平移、
 * 双击就地改标题、选中节点右侧「+」按钮与 Tab 新建关联节点、顶部搜索框定位节点。
 * （右上角原本还有「展示全部」与「一键整理布局」两个按钮，2026-10-07 都删掉了：
 * 缩放与平移用滚轮 / 拖拽就够了，整理则会把用户自己摆好的位置冲掉。）
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
  /** 选中的节点：单选时里面只有一个，框选可以有一批。与连线互斥（选中连线时它为空） */
  private selectedNodes = new Set<number>();
  private selectedLineId: number | null = null;
  /** 是否按住 Ctrl（平移键之一，见 panDown） */
  private ctrlDown = false;
  /** 是否按住空格（平移键之一，见 panDown） */
  private spaceDown = false;
  /** 正在框选：起点、跟着指针长的那个矩形、以及有没有真的拖出距离（没拖动就还是单击） */
  private marquee: { origin: Konva.Vector2d; rect: Konva.Rect; moved: boolean } | null = null;
  /** 框完选的那一下别再当成「点空白」：Konva 在 mouseup 之后还会补一个 click */
  private skipStageClick = false;
  /** 多选拖拽的基线：拖其中一个，整批按同一个位移走 */
  private groupDrag: {
    id: number;
    origin: Konva.Vector2d;
    members: Map<number, { x: number; y: number }>;
  } | null = null;
  /** 连线 id → 它那条 Path，选中换样式与右键认线都要按 id 反查 */
  private lineShapes = new Map<number, Konva.Path>();
  /** 正在就地改标题的节点 id（非 null 时画布不平移，见 beginTitleEdit） */
  private editingNodeId: number | null = null;
  /** 搜不到节点时那条提示的消失计时器（连着搜两次只认最后一次） */
  private tipTimer: number | null = null;
  /** 正在从某个节点往外拉线（非空时这个节点与画布都不再被拖动） */
  private linking: { sourceId: number } | null = null;
  /** 拉线时跟着指针走的那条预览虚线，松手就删 */
  private tempLink: Konva.Path | null = null;
  /** 预览线这会儿指着哪个节点（高亮它，表示「松手就连到它」） */
  private linkTargetId: number | null = null;

  constructor() {
    super(html);
  }

  override ready(): void {
    const container = this.dom.querySelector<HTMLDivElement>(".knowNetCanvas")!;
    this.stage = new Konva.Stage({ container, width: container.clientWidth, height: container.clientHeight });
    this.lineLayer = new Konva.Layer();
    this.nodeLayer = new Konva.Layer();
    this.stage.add(this.lineLayer, this.nodeLayer);
    // 画布**默认不能拖**：只有按住 Ctrl 拖空白才平移，不按 Ctrl 拖空白是框选（见 updateStageDraggable）
    this.stage.draggable(false);

    // 空白 + 左键 + 没按平移键 = 起一次框选；命中节点 / 连线时不动（那是拖节点或选中它）
    this.stage.on("mousedown", (e) => {
      if (e.evt.button !== 0) return;
      // 新的一次按下：上一轮留下的"别取消选中"一律作废。拖完节点 Konva 不补 click，
      // 那个标志会一直挂着，把下一次点空白（本该取消选中）白吃掉一次
      this.skipStageClick = false;
      if (e.target !== this.stage || this.panDown) return;
      this.beginMarquee();
    });
    // 平移过画布之后 Konva 还会补一个 click：那不是"点空白取消选中"。
    // 只认 stage 自己那次 dragend：拖节点冒泡上来的不算（那之后没有 click 要吞）
    this.stage.on("dragend", (e) => {
      if (e.target === this.stage) this.skipStageClick = true;
    });

    // 面板宽度会被 splitter 拖动、窗口也会被拉伸：跟着容器改 stage 尺寸，否则画布不跟着变
    new ResizeObserver(() => this.syncSize()).observe(container);

    // 滚轮：**Ctrl + 滚**才缩放（以鼠标为锚点，光标下那个点缩放前后停在同一处）；
    // 不按 Ctrl 滚 = 上下挪画布，就是滚动条的用法。触控板的捏合缩放浏览器会带 ctrlKey，天然落在缩放这条上
    this.stage.on("wheel", (e) => {
      e.evt.preventDefault(); // 不交给页面：整个画布吃掉滚轮
      // 正在改标题时滚了滚轮：先收掉编辑，输入框不会跟着画布一起缩放平移
      if (this.editingNodeId != null) void this.endTitleEdit(true);
      if (e.evt.ctrlKey) {
        this.zoomBy(e.evt.deltaY < 0 ? SCALE_STEP : 1 / SCALE_STEP, this.stage!.getPointerPosition());
        return;
      }
      this.scrollBy(e.evt);
    });

    // 点空白处（事件的 target 就是 stage 本身）= 取消选中，KnowDetail 随之关掉。
    // 框选刚框完 / 刚平移完画布时跳过这一次：Konva 会补一个 click，那不是用户想取消选中。
    // 这个标志只在"这一次按下 → 抬起"之内有效，下次按下就被清掉（见 mousedown）
    this.stage.on("click", (e) => {
      if (this.skipStageClick) return void (this.skipStageClick = false);
      if (e.target === this.stage) this.selectNode(null);
    });

    // 右键：命中连线 → 挑颜色 + 删连线；命中节点 → 挑颜色 + 删节点；命中空白 → 不弹
    // （浏览器的默认菜单也一起拦掉，只走我们自己的这个 DOM 菜单）
    this.stage.on("contextmenu", (e) => {
      e.evt.preventDefault();
      const evt = e.evt as MouseEvent;
      const lineId = this.lineIdOfShape(e.target);
      if (lineId != null) {
        this.selectLine(lineId); // 先选中（详情面板跟着换过去），再弹菜单
        const line = this.lines.find((l) => l.id === lineId);
        Menu.open(evt.clientX, evt.clientY, [
          this.colorRow(line?.color ?? 0, (color) => void this.markLine(lineId, color)),
          { label: "删除连线", onSelect: () => void this.removeLine(lineId) },
        ]);
        return;
      }
      const nodeId = this.nodeIdOfShape(e.target);
      if (nodeId != null) {
        // 右键的正是多选里的一个：保留这一批（不清成单选），删也是删整批
        const batch = this.selectedNodes.has(nodeId) ? [...this.selectedNodes] : (this.selectNode(nodeId), [nodeId]);
        const node = this.nodes.find((n) => n.id === nodeId);
        Menu.open(evt.clientX, evt.clientY, [
          this.colorRow(node?.color ?? 0, (color) => void this.markNode(nodeId, color)),
          {
            label: batch.length > 1 ? `删除选中的 ${batch.length} 个节点` : "删除节点",
            onSelect: () => void this.removeNodes(batch),
          },
        ]);
        return;
      }
      // 空白处：新建一个节点，就放在右键这个地方（没打开任何知识时无处可建，不弹）
      const point = this.toCanvasPoint(evt.clientX, evt.clientY);
      if (this.listId == null || !point) return void Menu.close();
      Menu.open(evt.clientX, evt.clientY, [
        { label: "新建知识节点", onSelect: () => void this.createNodeAt(point) },
      ]);
    });

    // 顶部搜索框：回车 / 点右边的搜索图标 = 按标题找节点，命中第一个就挪到视口中心并选中
    const searchInput = this.searchInput;
    searchInput.addEventListener("keydown", (e) => {
      if (e.key !== "Enter") return;
      e.preventDefault(); // 没有 form，只是不想让回车有任何默认动作
      this.searchNode(searchInput.value);
    });
    this.dom.querySelector<HTMLElement>(".knowNetSearchBtn")!.addEventListener("click", () => {
      this.searchNode(searchInput.value);
    });

    // 标题输入框：回车 / 失焦 = 提交，Esc = 放弃，Tab = 提交并接着建下一个关联节点。
    // 按键不外传，免得画布的快捷键（Tab / Delete）收到
    this.titleEditor.addEventListener("keydown", (e) => {
      e.stopPropagation();
      if (e.key === "Enter") void this.endTitleEdit(true);
      else if (e.key === "Escape") void this.endTitleEdit(false);
      else if (e.key === "Tab") {
        // 连打：当前这个先落库，然后马上建一个关联节点并接着编辑它的标题
        e.preventDefault(); // 不按的话焦点会跳到下一个可聚焦元素
        void this.tabToRelatedNode();
      }
    });
    this.titleEditor.addEventListener("blur", () => void this.endTitleEdit(true));

    // 指针动一下：拉线的预览线要重画，框选的框也要跟着长
    this.stage.on("mousemove", () => {
      this.updateMarquee();
      this.updateTempLink();
    });
    // 松手挂在 window 上：拖到画布外面再松手也能收尾
    window.addEventListener("mouseup", this.onWindowMouseUp);

    // Tab = 以当前选中的节点为起点新建一个关联节点；Del = 删选中的节点 / 连线
    document.addEventListener("keydown", this.onKeyDown);
    // 平移键的按下 / 松开要单独盯：它决定"拖空白"是平移还是框选
    document.addEventListener("keyup", this.onKeyUp);
    // 切走窗口时按键的 keyup 收不到，会一直以为平移键还按着
    window.addEventListener("blur", () => {
      this.setPanKey("ctrl", false);
      this.setPanKey("space", false);
    });
  }

  /** 加载某个知识的节点与连线并画出来（点列表项时由 KnowList 调用） */
  async open(listId: number): Promise<void> {
    void this.endTitleEdit(true); // 换一张网之前把正在改的标题落库
    this.listId = listId;
    this.searchBox.classList.add("show"); // 有知识可搜了才把搜索框放出来
    this.selectNode(null);
    // 换知识：面板内容切到这个知识本身（选中集合没变时 selectNode(null) 会提前返回，所以这里显式切一次）
    void KnowDetail.showList(listId);
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
    // 矩形宽了 / 窄了，「+」按钮要重新贴到新的右边缘外（不然它就压在节点上了）
    this.placeAddButton(group, width);
    this.redrawLines();
  }

  /** 清空画布（删掉当前打开的知识、或没有任何知识时） */
  clear(): void {
    void this.endTitleEdit(true);
    this.listId = null;
    this.nodes = [];
    this.lines = [];
    this.selectNode(null);
    // 知识都没了：搜索框收起来（搜无可搜），上一次的搜索词与提示也一并清掉
    this.searchBox.classList.remove("show");
    this.searchInput.value = "";
    this.hideTip();
    void KnowDetail.showList(null); // 没有知识了：面板没有可显示的对象，收起
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
    this.abortLink(); // 重画会把预览线一起清掉，先按规矩收尾（恢复被临时关掉的拖拽）
    this.nodeLayer?.destroyChildren();
    this.groups.clear();
    this.redrawLines();

    for (const node of this.nodes) {
      const group = this.buildNode(node);
      this.groups.set(node.id, group);
      this.nodeLayer?.add(group);
    }
    // 形状是新建的，选中态得补回去（蓝边 + 单选的「+」按钮）
    for (const id of this.selectedNodes) this.setHighlight(id, true);
  }

  /**
   * 连线的路径：两个节点中心之间的**直线**（2026-10-06 由贝塞尔改回直线——曲线不好看）。
   * 端点取中心而不是边缘：节点矩形画在连线的上层，盖住的那一段正好让线看起来从边缘出来，
   * 也省得按节点宽高去算交点（标题一改宽度就变）。
   */
  private linePath(a: { x: number; y: number }, b: { x: number; y: number }): string {
    return `M ${a.x} ${a.y} L ${b.x} ${b.y}`;
  }

  /**
   * 一条连线。它现在是**可以点、可以右键**的：单击选中（详情面板切到它），右键弹菜单删它。
   * 线本身只有 LINE_WIDTH 那么宽，判定放宽到 12px 才点得中（点击热区与视觉粗细是两回事，
   * 线细了也要照样点得中）；`name: "line"` 用来在事件里认出它。
   */
  private buildLine(a: NodeData, b: NodeData, id: number): Konva.Path {
    const line = this.lines.find((l) => l.id === id);
    const path = new Konva.Path({
      data: this.linePath(a, b),
      stroke: this.lineStroke(line?.color ?? 0, false), // 刚建出来的线不可能是选中态
      strokeWidth: LINE_WIDTH,
      lineCap: "round",
      hitStrokeWidth: LINE_HIT_WIDTH,
      name: "line",
    });
    path.on("click", (e) => {
      // 别冒泡到 stage：stage 收到 click 会当成"点了空白"，把刚选中的又取消掉
      e.cancelBubble = true;
      this.selectLine(id);
    });
    path.on("mouseenter", () => this.setCursor("pointer"));
    path.on("mouseleave", () => {
      if (!this.linking) this.setCursor("");
    });
    return path;
  }

  /**
   * 一个节点 = 圆角矩形 + 居中标题。宽度按标题长度自适应，夹在 [NODE_MIN_WIDTH, NODE_MAX_WIDTH]，
   * 文字超出用省略号（悬停/选中看全文由 title 与显示层保证，见 Arch/33）。
   */
  private buildNode(node: NodeData): Konva.Group {
    const measure = new Konva.Text({ text: node.title, fontSize: NODE_FONT_SIZE });
    const width = Math.min(NODE_MAX_WIDTH, Math.max(NODE_MIN_WIDTH, measure.width() + NODE_PADDING));
    const group = new Konva.Group({ name: "nodeGroup", x: node.x, y: node.y, draggable: true });

    const rect = new Konva.Rect({
      x: -width / 2,
      y: -NODE_HEIGHT / 2,
      width,
      height: NODE_HEIGHT,
      fill: this.nodeFill(node.color ?? 0),
      stroke: "#d9dde3",
      strokeWidth: NODE_STROKE_WIDTH,
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
    const addBtn = this.buildAddButton();
    group.add(rect, text, addBtn);
    this.placeAddButton(group, width);

    // 边缘 = 往外拉线，中间 = 拖节点：按下时先分清是哪一种。
    // 挂在矩形上而不是整个 group 上：这样「+」按钮上的按下不会掺和进来。
    // 宽度读 rect.width() 而不是闭包里的 width：改标题后矩形会变宽（见 updateNodeTitle），热区要跟着走
    rect.on("mousedown", (e) => {
      const pos = group.getRelativePointerPosition();
      if (!pos || !this.isEdgeHit(pos, rect.width())) return;
      e.cancelBubble = true; // 别让 stage 也以为是要拖它
      this.beginLink(node.id);
    });
    // 悬停在边缘上就把光标换成十字，提示这里可以拉线
    rect.on("mousemove", () => {
      if (this.linking) return;
      const pos = group.getRelativePointerPosition();
      this.setCursor(pos && this.isEdgeHit(pos, rect.width()) ? "crosshair" : "move");
    });
    rect.on("mouseleave", () => {
      if (!this.linking) this.setCursor("");
    });

    // 按下时先定好"这一下要拖谁"：已经在这一批里就留着整批（dragstart 会带着它们一起走），
    // 不在就先单选它——不然拖的是个没选中的节点，看起来像选中框失灵。
    // **Ctrl + 点 = 加选 / 取消这一个**（多选的第二种方式，见 toggleNode）：
    // 判断放在 mousedown 而不是 click，是因为拖起来之后 Konva 就不补 click 了，
    // "Ctrl 按住拖一下"也得先把这一个加进这一批，才能带着整批一起走
    group.on("mousedown", (e) => {
      if (this.linking) return;
      if (e.evt.ctrlKey) return void this.toggleNode(node.id);
      if (!this.selectedNodes.has(node.id)) this.selectNode(node.id);
    });
    group.on("click", (e) => {
      // Ctrl 那一下已经在 mousedown 里 toggle 过了：这里再选一次等于把它又翻回去
      if (e.evt.ctrlKey) return;
      this.selectNode(node.id);
    });
    // 双击 = 就地改标题：在这块覆盖一个 DOM 输入框（Konva 里画不出能打字的东西，
    // 而且 DOM 输入框才能吃到中文输入法的候选过程）
    group.on("dblclick", () => this.beginTitleEdit(node.id));
    // 拖拽结束才写库：拖动过程中每次移动都写一次没有必要，也没有意义
    group.on("dragstart", () => {
      // 兜底：拉线的这一下要是被 Konva 认成了拖拽，立刻停掉（此刻位置还没变）
      if (this.linking) return void group.stopDrag();
      this.beginGroupDrag(node.id); // 多选时记下基线，好让整批跟着一起走
    });
    // 拖拽过程中就按实时位置重画连线，线跟着节点走（松手才改变会显得"线断了"）。
    // 节点坐标与写库仍留到 dragend：拖动中每动一下都写一次库没有意义
    group.on("dragmove", () => {
      if (this.linking) return; // 拉线：这一下"拖拽"不算数
      node.x = group.x();
      node.y = group.y();
      this.applyGroupDrag(); // 多选：同批其余节点按同一个位移跟上
      this.updateLines();
    });
    group.on("dragend", () => {
      if (this.linking) return; // 拉线：这一下"拖拽"不算数，别写库
      node.x = group.x();
      node.y = group.y();
      this.applyGroupDrag();
      // 多选时这一批都要写库（单选就只有它自己）
      for (const id of this.groupDrag?.members.keys() ?? [node.id]) {
        const item = this.nodes.find((n) => n.id === id);
        if (item) void Msg.invoke("node.move", { id, x: item.x, y: item.y });
      }
      this.groupDrag = null;
      this.redrawLines();
    });
    return group;
  }

  /**
   * 只把已有连线挪到新路径上，**不重建形状**：拖拽这种每帧都要走的场合用它。
   * redrawLines 会销毁并新建所有 Path（连带重建事件与选中态），每帧来一遍是白费。
   */
  private updateLines(): void {
    for (const line of this.lines) {
      const shape = this.lineShapes.get(line.id);
      if (!shape) continue;
      const a = this.nodes.find((n) => n.id === line.nodeAId);
      const b = this.nodes.find((n) => n.id === line.nodeBId);
      if (!a || !b) continue;
      shape.data(this.linePath(a, b));
    }
  }

  /**
   * 重画全部连线：节点挪位置、标题改宽度、增删连线之后都要来一遍（线是按坐标算出来的，不随节点走）。
   * 顺带重建「id → Path」的映射，并把选中态补回新画出来的那条线上。
   */
  private redrawLines(): void {
    if (!this.lineLayer) return;
    this.lineLayer.destroyChildren();
    this.lineShapes.clear();
    for (const line of this.lines) {
      const a = this.nodes.find((n) => n.id === line.nodeAId);
      const b = this.nodes.find((n) => n.id === line.nodeBId);
      if (!a || !b) continue;
      const path = this.buildLine(a, b, line.id);
      this.lineShapes.set(line.id, path);
      this.lineLayer.add(path);
    }
    if (this.selectedLineId != null) this.paintLine(this.selectedLineId);
  }

  /** 唯一选中的那个节点；多选或没选中时为 null（详情面板、Tab、「+」按钮都只在单选时有意义） */
  private get soleNodeId(): number | null {
    return this.selectedNodes.size === 1 ? [...this.selectedNodes][0] : null;
  }

  /**
   * 选中一批节点（空 = 取消选中）：换高亮，并按选中个数决定要不要展开详情。
   * 只有**恰好选中一个**时才交给 KnowDetail——详情是给「某一个对象」用的，多选时不展开。
   * 选中集合在这里先落定再刷高亮：「+」按钮的可见性要按新的单选 / 多选状态算。
   */
  private selectNodes(ids: Iterable<number>): void {
    // 节点与连线互斥：选中一个就把另一个清掉（详情面板只有一个，StatusBar 也只有一行）
    const next = new Set(ids);
    const previous = this.selectedNodes;
    const unchanged = previous.size === next.size && [...next].every((id) => previous.has(id));
    if (unchanged && this.selectedLineId == null) return; // 一个没变就别动：省得详情被重新加载一遍
    if (this.selectedLineId != null) this.selectLine(null);
    this.selectedNodes = next;
    for (const id of previous) if (!next.has(id)) this.setHighlight(id, false);
    for (const id of next) this.setHighlight(id, true);
    this.syncSelectionUi();
  }

  /** 选中单个节点（传 null = 取消选中）；框选那一批走 selectNodes */
  private selectNode(id: number | null): void {
    this.selectNodes(id == null ? [] : [id]);
  }

  /**
   * Ctrl + 点一个节点：在这批里就去掉，不在就加进来——**多选的第二种方式**（第一种是框选）。
   * 全部去掉之后就是"一个都没选中"，`syncSelectionUi` 会顺手把详情面板收回去。
   */
  private toggleNode(id: number): void {
    const next = new Set(this.selectedNodes);
    if (!next.delete(id)) next.add(id); // delete 返回 false 说明它本来就不在这批里 → 加进来
    this.selectNodes(next);
  }

  /**
   * 选中集合变了之后统一刷一遍状态栏与详情面板。
   * 详情面板**不会因为选中而自动展开**（展开是用户点按钮的事，见 KnowDetail），
   * 这里只负责把面板内容切到对应的对象上。
   */
  private syncSelectionUi(): void {
    const sole = this.soleNodeId;
    if (sole != null) {
      StatusBar.setSelection(this.nodes.find((n) => n.id === sole)?.title ?? "");
      void KnowDetail.showNode(sole);
      return;
    }
    // 多选 / 一个都没选中：面板内容回到「这个知识本身」的描述（没打开知识就收起面板）
    StatusBar.setSelection(this.selectedNodes.size ? `已选中 ${this.selectedNodes.size} 个节点` : "");
    void KnowDetail.showList(this.listId);
  }

  /** 选中某条连线：它没有标题，只有详情，KnowDetail 按 line 打开 */
  private selectLine(id: number | null): void {
    if (this.selectedNodes.size) this.selectNodes([]);
    if (this.selectedLineId === id) return;
    const previous = this.selectedLineId;
    if (previous != null) this.paintLine(previous);
    this.selectedLineId = id;
    if (id != null) this.paintLine(id);

    if (id == null) {
      // 连线不选了：面板内容回到「这个知识本身」（没打开知识就收起面板）
      void KnowDetail.showList(this.listId);
      StatusBar.setSelection("");
      return;
    }
    const line = this.lines.find((l) => l.id === id);
    StatusBar.setSelection(line ? this.lineLabel(line) : "");
    void KnowDetail.showLine(id);
  }

  /**
   * 按"当前是否选中 + 标记色"重画一条线的颜色，粗细恒为 LINE_WIDTH。
   * 选中态**不能**一律改回蓝色：右键挑完颜色时这条线正被选中，改蓝就等于刚挑的颜色看不见了。
   */
  private paintLine(id: number): void {
    const path = this.lineShapes.get(id);
    if (!path) return;
    const line = this.lines.find((l) => l.id === id);
    path.stroke(this.lineStroke(line?.color ?? 0, this.selectedLineId === id));
    path.strokeWidth(LINE_WIDTH);
    this.lineLayer?.batchDraw();
  }

  /** 状态栏上怎么称呼一条连线：两端节点的标题（连线自己没有标题） */
  private lineLabel(line: LineData): string {
    const a = this.nodes.find((n) => n.id === line.nodeAId)?.title ?? "";
    const b = this.nodes.find((n) => n.id === line.nodeBId)?.title ?? "";
    return `${a} ↔ ${b}`;
  }

  /** 节点矩形的填充色：没标记是白的，标记了用该颜色本身——**不加透明度**，
   *  否则后面压着的连线会从节点里透出来，很难看 */
  private nodeFill(color: number): string {
    return MARK_COLORS[color - 1] ?? "#ffffff";
  }

  /**
   * 连线的颜色：没标记时「默认灰 / 选中蓝」，标记了用标记色本身（选中压暗一档当选中反馈）。
   * 连线同样不设透明度。
   */
  private lineStroke(color: number, selected: boolean): string {
    const hex = MARK_COLORS[color - 1];
    if (!hex) return selected ? "#1677ff" : "#b6bac1";
    return selected ? darken(hex, 0.75) : hex;
  }

  /** 按当前标记色重画节点的填充（边框与选中态归 setHighlight 管） */
  private paintNode(id: number): void {
    const node = this.nodes.find((n) => n.id === id);
    const rect = this.groups.get(id)?.findOne<Konva.Rect>("Rect");
    if (!node || !rect) return;
    rect.fill(this.nodeFill(node.color ?? 0));
    this.nodeLayer?.batchDraw();
  }

  /** 给节点打标记色：先写库，成功才改本地并重画（写失败就保持原样，下一轮还会再写） */
  private async markNode(id: number, color: number): Promise<void> {
    const node = this.nodes.find((n) => n.id === id);
    if (!node || (node.color ?? 0) === color) return;
    try {
      await Msg.invoke("node.color", { id, color });
    } catch {
      return;
    }
    node.color = color;
    this.paintNode(id);
  }

  /** 给连线打标记色，与 markNode 同样的套路 */
  private async markLine(id: number, color: number): Promise<void> {
    const line = this.lines.find((l) => l.id === id);
    if (!line || (line.color ?? 0) === color) return;
    try {
      await Msg.invoke("line.color", { id, color });
    } catch {
      return;
    }
    line.color = color;
    this.paintLine(id);
  }

  /**
   * 右键菜单里的那一行颜色按钮（菜单只有**一项**，六色并排）：当前色是 icon-check，其余 icon-uncheck；
   * 再点一次当前色的按钮 = 取消标记，回到默认（color 0）。
   */
  private colorRow(current: number, onPick: (color: number) => void): MenuItem {
    return {
      colors: MARK_COLORS.map((hex, i) => ({
        color: hex,
        checked: current === i + 1,
        title: MARK_NAMES[i],
        onSelect: () => onPick(current === i + 1 ? 0 : i + 1),
      })),
    };
  }

  private setHighlight(id: number, on: boolean): void {
    const group = this.groups.get(id);
    const rect = group?.findOne<Konva.Rect>("Rect");
    if (!group || !rect) return;
    // 选中与否只换颜色，不换粗细：两种状态都是 1px（2026-10-07 改定）
    rect.stroke(on ? "#1677ff" : "#d9dde3");
    rect.strokeWidth(NODE_STROKE_WIDTH);
    // 「+」按钮只属于单选的那一个节点：多选时谁都不带（它会作用于"当前选中的那个"，
    // 而多选没有"那一个"）；拉线时高亮别的节点也不该把它的按钮带出来
    group.findOne<Konva.Group>(".addBtn")?.visible(on && id === this.soleNodeId);
  }

  /**
   * 把「+」按钮贴到节点右边缘外：按钮中心离边缘 ADD_BUTTON_OFFSET，圆自身的半径不算进去，
   * 所以圆与节点之间还留着 ADD_BUTTON_OFFSET - ADD_BUTTON_RADIUS 的空隙。
   * 标题改长 / 改短时矩形宽度会变（见 updateNodeTitle），必须重新贴一次，否则按钮会留在旧位置上、
   * 看起来像压在节点上。
   */
  private placeAddButton(group: Konva.Group, width: number): void {
    group.findOne<Konva.Group>(".addBtn")?.x(width / 2 + ADD_BUTTON_OFFSET);
  }

  /** 选中节点右侧的「+」按钮：点它新建一个与它关联的节点（只在选中时显示） */
  private buildAddButton(): Konva.Group {
    const btn = new Konva.Group({ name: "addBtn", y: 0, visible: false });
    btn.add(
      new Konva.Circle({ radius: ADD_BUTTON_RADIUS, fill: "#1677ff" }),
      // 加号：一横一竖，两条线都不吃事件（点击判定交给外层的圆）
      new Konva.Line({ points: [-4, 0, 4, 0], stroke: "#fff", strokeWidth: 1.6, lineCap: "round", listening: false }),
      new Konva.Line({ points: [0, -4, 0, 4], stroke: "#fff", strokeWidth: 1.6, lineCap: "round", listening: false }),
    );
    btn.on("click", (e) => {
      // 别冒泡到节点上再走一次选中流程（会把详情面板重新加载一遍）
      e.cancelBubble = true;
      const source = this.soleNodeId; // 多选时不显示这个按钮，所以走到这里必是单选
      if (source != null) void this.createRelatedNode(source);
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
   * 从节点边缘往外拖 = 拉一条新连线（Arch/32）。
   * 与拖节点的区别只在按下的位置：落在边缘热区里就是拉线。
   * Konva 的拖拽是在 mousedown 时认领的，所以这里当场把节点与画布的 draggable 关掉，
   * 让这一下只被当成拉线（dragstart 里还有一道兜底）。
   */
  private beginLink(sourceId: number): void {
    if (this.linking) return;
    const source = this.nodes.find((n) => n.id === sourceId);
    if (!source) return;
    this.groups.get(sourceId)?.draggable(false);
    this.linking = { sourceId };
    this.updateStageDraggable(); // 拉线期间不许平移画布（Ctrl / 空格按着也不行）
    this.tempLink = new Konva.Path({
      stroke: "#1677ff",
      strokeWidth: 1.5,
      dash: [6, 4],
      lineCap: "round",
      listening: false,
    });
    this.lineLayer?.add(this.tempLink);
    this.updateTempLink();
  }

  /** 预览线跟着指针走：起点是源节点中心，终点是指针在画布里的位置 */
  private updateTempLink(): void {
    const link = this.linking;
    if (!link || !this.tempLink || !this.stage) return;
    const source = this.nodes.find((n) => n.id === link.sourceId);
    const point = this.stage.getRelativePointerPosition();
    if (!source || !point) return;
    this.tempLink.data(this.linePath(source, point));
    this.lineLayer?.batchDraw();
    // 指针底下的节点高亮一下，告诉用户「松手就连到它」
    this.updateLinkTarget(this.nodeIdAtPointer());
  }

  /** 松手：底下是个合法的节点就建线，其余（空白 / 拖到自己 / 已经连过）一律无声取消 */
  private finishLink(): void {
    const link = this.linking;
    if (!link) return;
    const targetId = this.nodeIdAtPointer();
    this.abortLink();
    if (targetId == null || targetId === link.sourceId) return;
    const duplicated = this.lines.some(
      (l) =>
        (l.nodeAId === link.sourceId && l.nodeBId === targetId) ||
        (l.nodeAId === targetId && l.nodeBId === link.sourceId),
    );
    if (duplicated) return;
    void this.createLine(link.sourceId, targetId);
  }

  /** 收掉这次拉线的一切痕迹：预览线、目标高亮、以及被临时关掉的拖拽 */
  private abortLink(): void {
    const link = this.linking;
    if (!link) return;
    this.linking = null;
    this.tempLink?.destroy();
    this.tempLink = null;
    this.updateLinkTarget(null);
    this.groups.get(link.sourceId)?.draggable(true);
    this.updateStageDraggable(); // 拉完了：能不能拖画布重新交回给平移键
  }

  /** 建一条连线（拉线松手、+ / Tab 新建关联节点都走这里） */
  private async createLine(sourceId: number, targetId: number): Promise<void> {
    try {
      const lineId = (await Msg.invoke("line.create", { nodeAId: sourceId, nodeBId: targetId })) as number;
      if (!lineId) return;
      this.lines.push({ id: lineId, nodeAId: sourceId, nodeBId: targetId, color: 0 });
      this.redrawLines();
    } catch {
      // 建不出来就算了：画布保持原样（连不上用户可以再拖一次）
    }
  }

  /** 指针（相对节点中心）是不是落在边缘热区里：热区宽度按缩放折算，屏幕上恒定那么宽 */
  private isEdgeHit(pos: Konva.Vector2d, width: number): boolean {
    const scale = this.stage?.scaleX() ?? 1;
    const hot = EDGE_HOT_ZONE / scale;
    return Math.abs(pos.x) > width / 2 - hot || Math.abs(pos.y) > NODE_HEIGHT / 2 - hot;
  }

  /** 指针底下是哪个节点 */
  private nodeIdAtPointer(): number | null {
    const stage = this.stage;
    if (!stage) return null;
    const point = stage.getPointerPosition();
    if (!point) return null;
    return this.nodeIdOfShape(stage.getIntersection(point) ?? null);
  }

  /** 某个形状属于哪个节点：命中的可能是不参与交互的文字，往上找到节点 group 再反查 id */
  private nodeIdOfShape(shape: Konva.Node | null): number | null {
    const group = shape?.findAncestor(".nodeGroup", true) ?? null;
    if (!group) return null;
    for (const [id, candidate] of this.groups) if (candidate === group) return id;
    return null;
  }

  /** 某个形状是不是某条连线：连线在 lineLayer 里，与节点各占各的命中区域 */
  private lineIdOfShape(shape: Konva.Node | null): number | null {
    if (!shape || shape.name() !== "line") return null;
    for (const [id, path] of this.lineShapes) if (path === shape) return id;
    return null;
  }

  /**
   * 删一批节点（单选、多选、右键菜单都走这里）：库里会连带删掉它们的连线与详情，
   * 前端把对应的本地数据一起清掉。最后只重画一次——逐个删逐个重画会闪。
   */
  private async removeNodes(ids: number[]): Promise<void> {
    for (const id of ids) if (this.editingNodeId === id) void this.endTitleEdit(false); // 这次编辑作废
    const removed = new Set<number>();
    for (const id of ids) {
      try {
        await Msg.invoke("node.remove", { id });
        removed.add(id);
      } catch {
        // 删不掉就留着它：画布与库保持一致更重要
      }
    }
    if (!removed.size) return;
    this.nodes = this.nodes.filter((n) => !removed.has(n.id));
    // 挂在它们身上的连线随它们一起没了（库里是外键级联）
    this.lines = this.lines.filter((l) => !removed.has(l.nodeAId) && !removed.has(l.nodeBId));
    // 删掉的要从选中集合里剔掉；这一批全删完就什么都没选中了
    this.selectedNodes = new Set([...this.selectedNodes].filter((id) => !removed.has(id)));
    this.render(); // 重画会把选中态按 selectedNodes 补回去
    this.syncSelectionUi();
    StatusBar.setCount(this.nodes.length);
  }

  /** 删连线：只删它自己，两端节点与各自的详情都留着 */
  private async removeLine(id: number): Promise<void> {
    try {
      await Msg.invoke("line.remove", { id });
    } catch {
      return;
    }
    this.lines = this.lines.filter((l) => l.id !== id);
    if (this.selectedLineId === id) this.selectLine(null);
    this.redrawLines();
  }

  /** 拉线过程中的目标高亮：与选中同色，但不带出「+」按钮（见 setHighlight） */
  private updateLinkTarget(id: number | null): void {
    if (this.linkTargetId === id) return;
    const previous = this.linkTargetId;
    this.linkTargetId = id;
    // 选中的节点本来就是高亮的，别把它的高亮当成"拉线目标"给关掉
    if (previous != null && !this.selectedNodes.has(previous)) this.setHighlight(previous, false);
    if (id != null && !this.selectedNodes.has(id)) this.setHighlight(id, true);
  }

  /** 松手挂在 window 上：拖到画布外面再松手也要能收尾（把最后的位置补记给 stage） */
  private readonly onWindowMouseUp = (e: MouseEvent): void => {
    if (this.marquee != null) {
      this.stage?.setPointersPositions(e);
      this.finishMarquee();
      return;
    }
    if (!this.linking) return;
    this.stage?.setPointersPositions(e);
    this.finishLink();
  };

  /* ───────────── 框选：不按 Ctrl 拖空白 = 拉个框，框住的节点一起选中 ───────────── */

  /** 起一次框选：记下起点，铺一个跟着指针长的半透明矩形 */
  private beginMarquee(): void {
    const stage = this.stage;
    const origin = stage?.getRelativePointerPosition();
    if (!stage || !origin || !this.nodeLayer) return;
    const scale = stage.scaleX();
    const rect = new Konva.Rect({
      x: origin.x,
      y: origin.y,
      width: 0,
      height: 0,
      fill: "rgba(22, 119, 255, 0.08)",
      stroke: "#1677ff",
      // 线宽与虚线按缩放折算：缩得再小，屏幕上看着也还是 1px 粗
      strokeWidth: 1 / scale,
      dash: [4 / scale, 3 / scale],
      listening: false, // 选框不参与命中：它盖住的节点还得能点到
    });
    this.nodeLayer.add(rect);
    this.marquee = { origin, rect, moved: false };
  }

  private updateMarquee(): void {
    const box = this.marquee;
    const point = this.stage?.getRelativePointerPosition();
    if (!box || !point) return;
    // 拖出 3px 才算框选；手抖一下还是"在空白处点了一下"，由 click 去取消选中
    if (Math.abs(point.x - box.origin.x) > 3 || Math.abs(point.y - box.origin.y) > 3) box.moved = true;
    box.rect.setAttrs({
      x: Math.min(box.origin.x, point.x),
      y: Math.min(box.origin.y, point.y),
      width: Math.abs(point.x - box.origin.x),
      height: Math.abs(point.y - box.origin.y),
    });
    this.nodeLayer?.batchDraw();
  }

  /** 松手：框住哪几个就选哪几个；没拖动过就什么都不做（那一下归 click 管） */
  private finishMarquee(): void {
    const box = this.marquee;
    if (!box) return;
    this.marquee = null;
    const frame = { x: box.rect.x(), y: box.rect.y(), width: box.rect.width(), height: box.rect.height() };
    box.rect.destroy();
    if (!box.moved) return;
    this.skipStageClick = true; // Konva 随后会补一个 click，别让它把刚选中的取消掉
    this.selectNodes(this.nodes.filter((node) => this.nodeHitsBox(node, frame)).map((node) => node.id));
    this.nodeLayer?.batchDraw();
  }

  /** 节点与框有没有交叠：擦到一点就算选中（要求整个框住太难框了） */
  private nodeHitsBox(node: NodeData, box: { x: number; y: number; width: number; height: number }): boolean {
    const rect = this.groups.get(node.id)?.findOne<Konva.Rect>("Rect");
    const width = rect?.width() ?? NODE_MIN_WIDTH;
    return (
      node.x - width / 2 < box.x + box.width &&
      node.x + width / 2 > box.x &&
      node.y - NODE_HEIGHT / 2 < box.y + box.height &&
      node.y + NODE_HEIGHT / 2 > box.y
    );
  }

  /* ───────────── 多选拖拽：拖其中一个，整批按同一个位移走 ───────────── */

  /** 拖起某个节点时记下基线：它属于多选那一批就把整批的起始坐标都存下来 */
  private beginGroupDrag(id: number): void {
    if (!this.selectedNodes.has(id) || this.selectedNodes.size < 2) return; // 单选照旧，不必多此一举
    const members = new Map<number, { x: number; y: number }>();
    for (const node of this.nodes) {
      if (!this.selectedNodes.has(node.id)) continue;
      const group = this.groups.get(node.id);
      members.set(node.id, { x: group?.x() ?? node.x, y: group?.y() ?? node.y });
    }
    const origin = members.get(id)!;
    this.groupDrag = { id, origin, members };
  }

  /** 拖拽过程中：同批的其余节点按被拖那一个的位移跟上（它自己的位置 Konva 已经在改了） */
  private applyGroupDrag(): void {
    const drag = this.groupDrag;
    const group = drag && this.groups.get(drag.id);
    if (!drag || !group) return;
    const dx = group.x() - drag.origin.x;
    const dy = group.y() - drag.origin.y;
    for (const [id, base] of drag.members) {
      if (id === drag.id) continue;
      const x = base.x + dx;
      const y = base.y + dy;
      const node = this.nodes.find((n) => n.id === id);
      if (node) {
        node.x = x;
        node.y = y;
      }
      this.groups.get(id)?.position({ x, y });
    }
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
    this.adoptNewNode(nodeId, x, y);

    // 连线另外建：建不出来也留着这个节点（它已经入库了），用户可以再拖一次
    void this.createLine(sourceId, nodeId);
  }

  /** 右键空白处「新建知识节点」：就建在右键那个位置（Arch/32：坐标规则 = 右键处） */
  private async createNodeAt(point: Konva.Vector2d): Promise<void> {
    if (this.listId == null) return;
    try {
      const nodeId = (await Msg.invoke("node.create", { listId: this.listId, x: point.x, y: point.y })) as number;
      if (nodeId) this.adoptNewNode(nodeId, point.x, point.y);
    } catch {
      // 建不出来：画布保持原样
    }
  }

  /** 新节点入库之后这一段是共用的：进本地数据、重画、选中它并直接进入标题编辑（用户可以马上敲名字） */
  private adoptNewNode(nodeId: number, x: number, y: number): void {
    this.nodes.push({ id: nodeId, title: DEFAULT_TITLE, x, y, color: 0 });
    this.render();
    StatusBar.setCount(this.nodes.length);
    this.selectNode(nodeId);
    this.beginTitleEdit(nodeId);
  }

  /** 屏幕坐标 → 画布坐标（右键处新建节点要用；不依赖 Konva 有没有为 contextmenu 记录指针位置） */
  private toCanvasPoint(clientX: number, clientY: number): Konva.Vector2d | null {
    const stage = this.stage;
    if (!stage) return null;
    const rect = stage.container().getBoundingClientRect();
    const position = stage.position();
    const scale = stage.scaleX();
    return {
      x: (clientX - rect.left - position.x) / scale,
      y: (clientY - rect.top - position.y) / scale,
    };
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
    // 必须显式写 "block"：CSS 里 .knowNetTitleEdit 就是 display:none，
    // 设成 "" 只是清掉内联样式、回落到样式表的 none——输入框不显示，focus() 对不可见元素也无效
    input.style.display = "block";
    input.style.left = `${pos.x - width / 2}px`;
    input.style.top = `${pos.y - height / 2}px`;
    input.style.width = `${width}px`;
    input.style.height = `${height}px`;
    input.style.fontSize = `${NODE_FONT_SIZE * scale}px`;
    this.editingNodeId = id;
    this.updateStageDraggable(); // 编辑期间不许平移：输入框是 DOM，不会跟着画布走
    input.focus();
    input.select();
  }

  /**
   * 收掉输入框：commit = 把当前内容写回库（空串回落到「未命名」），false = 放弃这次编辑。
   * 返回的 promise 在**提交写库完成后**才 resolve——按 Tab 连打时要等这一下，
   * 因为源节点的矩形宽度是写完库才更新的，不等的话新节点会按旧宽度摆位、可能压在源节点身上。
   */
  private async endTitleEdit(commit: boolean): Promise<void> {
    const id = this.editingNodeId;
    if (id == null) return;
    this.editingNodeId = null;
    const value = this.titleEditor.value.trim();
    this.titleEditor.style.display = "none";
    this.updateStageDraggable(); // 编辑收掉了：能不能拖画布重新交回给 Ctrl
    if (commit) await this.commitTitle(id, value || DEFAULT_TITLE);
  }

  /**
   * 改标题时按 Tab：当前这个先落库，再以它为源新建一个关联节点。
   * 新节点建好后 `adoptNewNode()` 会自己进标题编辑并 focus，所以连打就是"敲名字 → Tab → 敲下一个名字"。
   */
  private async tabToRelatedNode(): Promise<void> {
    const sourceId = this.editingNodeId;
    if (sourceId == null) return;
    await this.endTitleEdit(true);
    await this.createRelatedNode(sourceId);
  }

  private async commitTitle(id: number, title: string): Promise<void> {
    const node = this.nodes.find((n) => n.id === id);
    if (!node || node.title === title) return;
    try {
      await Msg.invoke("node.update", { id, title });
      this.updateNodeTitle(id, title);
      // 标题的唯一入口就是画布上这个输入框，改完只需同步状态栏（详情面板里已经没有标题框了）
      StatusBar.setSelection(title);
    } catch {
      // 写失败：画布上的标题保持原样，下一次改动还会再写一次
    }
  }

  /**
   * 画布上的快捷键：**Tab** = 以选中节点为源新建关联节点；**Del** = 删掉选中的节点 / 连线
   * （选中了多个节点就删这一批）。**Ctrl 或空格**按住时拖空白 = 平移画布。
   * 焦点在能打字的东西里（详情编辑器、列表输入框等）时一律不抢，那些地方 Tab / Del 有它们自己的用处。
   */
  private readonly onKeyDown = (e: KeyboardEvent): void => {
    // Ctrl 的跟踪不受"焦点在输入框里"影响：它只是决定画布能不能拖
    if (e.key === "Control") this.setPanKey("ctrl", true);
    if (this.editingNodeId != null) return; // 正在就地改标题：按键归标题输入框管
    if (this.typingFocus()) return;

    // 空格：**能打字的地方一律不碰**（那就是在输空格）。这里吞掉默认行为有两个用处——
    // 不让页面滚动，也不让焦点停在某个按钮上时按空格把那个按钮点下去
    if (e.code === "Space") {
      e.preventDefault(); // 长按会重复 keydown，重复调也无妨（setPanKey 幂等）
      this.setPanKey("space", true);
      return;
    }

    if (e.key === "Tab") {
      const source = this.soleNodeId; // 多选时没有"新建一个关联节点"的落点，不响应
      if (source == null) return;
      e.preventDefault();
      void this.createRelatedNode(source);
      return;
    }
    if (e.key !== "Delete") return;
    // 选中的节点（一个或多个）优先；没有节点再看连线
    if (this.selectedNodes.size) {
      e.preventDefault();
      void this.removeNodes([...this.selectedNodes]);
    } else if (this.selectedLineId != null) {
      e.preventDefault();
      void this.removeLine(this.selectedLineId);
    }
  };

  /** 抬起：Ctrl / 空格松开就退出平移态（两个键都松开才算真的退出，见 panDown） */
  private readonly onKeyUp = (e: KeyboardEvent): void => {
    if (e.key === "Control") this.setPanKey("ctrl", false);
    else if (e.code === "Space") this.setPanKey("space", false);
  };

  /** **平移键**：Ctrl 与空格任一按住，就能拖着画布走 */
  private get panDown(): boolean {
    return this.ctrlDown || this.spaceDown;
  }

  /** 平移键按下 / 松开：它决定"拖空白"是平移画布还是框选 */
  private setPanKey(key: "ctrl" | "space", down: boolean): void {
    const before = this.panDown;
    if (key === "ctrl") this.ctrlDown = down;
    else this.spaceDown = down;
    if (this.panDown === before) return; // 组合没变（比如松了 Ctrl 但空格还按着）：别白刷一次
    this.updateStageDraggable();
  }

  /**
   * 画布**只在按住平移键（Ctrl 或空格）时**才允许拖着平移；都不按时拖空白是框选（见 beginMarquee）。
   * 正在拉线 / 就地改标题时一律不让平移——那两种操作自己要占着这一次拖拽。
   */
  private updateStageDraggable(): void {
    const draggable = this.panDown && this.linking == null && this.editingNodeId == null;
    this.stage?.draggable(draggable);
    if (this.panDown) this.setCursor("grab"); // 提示"现在可以拖画布了"
    else if (!this.linking) this.setCursor("");
  }

  /** 焦点是不是落在某个能打字的东西里（input / textarea / 富文本编辑器） */
  private typingFocus(): boolean {
    const active = document.activeElement;
    if (!active || active === document.body) return false;
    const el = active as HTMLElement;
    return el.tagName === "INPUT" || el.tagName === "TEXTAREA" || el.isContentEditable === true;
  }

  private get titleEditor(): HTMLInputElement {
    return this.dom.querySelector<HTMLInputElement>(".knowNetTitleEdit")!;
  }

  private get searchBox(): HTMLElement {
    return this.dom.querySelector<HTMLElement>(".knowNetSearch")!;
  }

  private get searchInput(): HTMLInputElement {
    return this.dom.querySelector<HTMLInputElement>(".knowNetSearchInput")!;
  }

  private get tipEl(): HTMLElement {
    return this.dom.querySelector<HTMLElement>(".knowNetTip")!;
  }

  /**
   * 按标题搜节点：命中**第一个**（`nodes` 的顺序就是库里取出来的顺序）就把它挪到视口正中并选中；
   * 一个都没命中就浮一条提示。空输入不算搜索——那等于什么条件都没给。
   * 大小写不敏感，前后空格不算数。
   */
  private searchNode(keyword: string): void {
    if (this.listId == null) return; // 没打开知识：搜索框是藏着的，这里再兜一道
    const text = keyword.trim().toLowerCase();
    if (!text) return;
    const hit = this.nodes.find((node) => node.title.toLowerCase().includes(text));
    if (!hit) return void this.showTip("没有找到匹配的节点");
    this.centerNode(hit.id);
    this.selectNode(hit.id);
  }

  /** 把某个节点摆到视口正中：只挪 stage 的位置（缩放不动），节点自己的 x / y 不变 */
  private centerNode(id: number): void {
    const stage = this.stage;
    const node = this.nodes.find((n) => n.id === id);
    if (!stage || !node) return;
    const scale = stage.scaleX();
    stage.position({ x: stage.width() / 2 - node.x * scale, y: stage.height() / 2 - node.y * scale });
  }

  /** 提示条浮出来，TIP_DURATION 之后自己没；连着搜两次只认最后一次（重开计时） */
  private showTip(text: string): void {
    const tip = this.tipEl;
    tip.textContent = text;
    tip.classList.add("show");
    if (this.tipTimer != null) clearTimeout(this.tipTimer);
    this.tipTimer = window.setTimeout(() => {
      tip.classList.remove("show");
      this.tipTimer = null;
    }, TIP_DURATION);
  }

  private hideTip(): void {
    if (this.tipTimer != null) clearTimeout(this.tipTimer);
    this.tipTimer = null;
    this.tipEl.classList.remove("show");
  }

  /** 视口中心对到画布原点：新建知识的第一个节点在 (0, 0)，于是它出现在正中 */
  private centerOrigin(): void {
    if (!this.stage) return;
    this.stage.scale({ x: 1, y: 1 });
    this.stage.position({ x: this.stage.width() / 2, y: this.stage.height() / 2 });
  }

  /**
   * 滚轮当滚动条用：往下滚（`deltaY > 0`）= 想看下面的内容 = 画布往上走，与页面滚动同向。
   * `deltaMode` 不都是像素：Firefox 常给「行」(1)，一页 (2) 罕见，都要折成像素再挪，
   * 不然同一只鼠标在不同浏览器里快慢差好几倍。
   */
  private scrollBy(evt: WheelEvent): void {
    if (!this.stage) return;
    const unit = evt.deltaMode === 1 ? WHEEL_LINE_HEIGHT : evt.deltaMode === 2 ? this.stage.height() : 1;
    const position = this.stage.position();
    this.stage.position({ x: position.x, y: position.y - evt.deltaY * unit });
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
