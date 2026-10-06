/**
 * 力导向布局：只算坐标，不碰 Konva（Arch/32 要求布局与绘制解耦）。
 *
 * 用法是**一次性**的：点「一键整理」时从当前坐标出发算到平衡，算出新坐标交给画布去做动画，
 * 不像 d3-force 那样持续演化。因此没有 alpha / reheat / 逐帧推进那一套，
 * 收敛判据就是"迭代次数跑完"。
 *
 * 算法是 Fruchterman-Reingold 的变体，改了三处：
 * - 斥力按**两个节点表面之间的距离**算，而不是中心距：节点越大越难被挤到一起；
 *   表面距离把矩形近似成同尺寸的**椭圆**（水平方向量到半宽、垂直方向量到半高）——
 *   直接取外接圆的话，220×36 这种扁节点会被当成直径 220 的圆，白占一大片地方；
 * - 加了一个很弱的**向心力**把整张图拉回原点，免得互不连通的那几块各自飘走；
 * - 算完把**质心挪到原点**：画布的约定是"视口中心对准画布原点"（Arch/32），
 *   只靠向心力挡不住整体漂移，不归位的话下次打开这张网就看不见了。
 *
 * 权重已删除（Arch/32），所以每条边的弹簧长度都一样：`IDEAL_DISTANCE`。
 */

export interface LayoutNode {
  id: number;
  x: number;
  y: number;
  /** 节点的半宽 / 半高：斥力按它们算，避免矩形叠在一起 */
  halfWidth: number;
  halfHeight: number;
}

export interface LayoutLine {
  nodeAId: number;
  nodeBId: number;
}

/** 弹簧自然长度：整理完之后有连线的两个节点大致相距这么远（Arch/32 草案 L = 180） */
const IDEAL_DISTANCE = 180;
/** 迭代次数：一次性算到底，跑完即视为收敛 */
const ITERATIONS = 300;
/** 初始位移上限（温度）：每步最多挪这么远，随迭代线性衰减，最后几步只做微调 */
const TEMPERATURE = 60;
/** 斥力里两个节点之间至少留出的空隙（在表面之外再留这么多） */
const PADDING = 40;
/** 向心力：只防飘散（尤其是没连线的孤立节点），不主导布局 */
const GRAVITY = 0.04;

/**
 * 算出力导向平衡后的坐标，返回「节点 id → 新坐标」。
 * 从**当前坐标**出发（用户已经摆过一遍，从这里收敛比从随机位置快，也不会无故翻个面）。
 */
export function forceLayout(nodes: LayoutNode[], lines: LayoutLine[]): Map<number, { x: number; y: number }> {
  const count = nodes.length;
  const pos = nodes.map((node) => ({ x: node.x, y: node.y }));
  const halfWidth = nodes.map((node) => node.halfWidth);
  const halfHeight = nodes.map((node) => node.halfHeight);

  const indexOf = new Map<number, number>();
  nodes.forEach((node, i) => indexOf.set(node.id, i));
  // 连线转成下标对：两端有一头不在本次数据里（脏数据）就跳过
  const edges: Array<[number, number]> = [];
  for (const line of lines) {
    const a = indexOf.get(line.nodeAId);
    const b = indexOf.get(line.nodeBId);
    if (a !== undefined && b !== undefined && a !== b) edges.push([a, b]);
  }

  const dx = new Float64Array(count);
  const dy = new Float64Array(count);

  for (let step = 0; step < ITERATIONS; step++) {
    dx.fill(0);
    dy.fill(0);

    // 斥力：每一对节点都互相推开（节点量级在数百，O(n²) 直算够用）
    for (let i = 0; i < count; i++) {
      for (let j = i + 1; j < count; j++) {
        let vx = pos[i].x - pos[j].x;
        let vy = pos[i].y - pos[j].y;
        let distance = Math.hypot(vx, vy);
        if (distance < 0.01) {
          // 完全重合：方向无从算起，按两个下标派生一个确定的小偏移掰开，
          // 于是同一份数据每次整理都得到同一个结果（而不是随机抖一下就换个样子）
          vx = ((i * 31 + j * 17) % 7) - 3;
          vy = ((i * 13 + j * 29) % 7) - 3;
          distance = Math.hypot(vx, vy) || 1;
        }
        const ux = vx / distance;
        const uy = vy / distance;
        // 表面之间还剩多少空隙：贴上了（gap 很小）斥力就猛涨，把两个节点推开
        const gap = Math.max(
          distance - ellipseRadius(ux, uy, halfWidth[i], halfHeight[i]) - ellipseRadius(ux, uy, halfWidth[j], halfHeight[j]) - PADDING,
          1,
        );
        const force = (IDEAL_DISTANCE * IDEAL_DISTANCE) / (gap * gap);
        dx[i] += ux * force;
        dy[i] += uy * force;
        dx[j] -= ux * force;
        dy[j] -= uy * force;
      }
    }

    // 引力：每条边把两端拉向 IDEAL_DISTANCE（拉得太近时力小，拉得太远时力大）
    for (const [a, b] of edges) {
      const vx = pos[a].x - pos[b].x;
      const vy = pos[a].y - pos[b].y;
      const distance = Math.max(Math.hypot(vx, vy), 0.01);
      const force = (distance * distance) / IDEAL_DISTANCE;
      const ux = vx / distance;
      const uy = vy / distance;
      dx[a] -= ux * force;
      dy[a] -= uy * force;
      dx[b] += ux * force;
      dy[b] += uy * force;
    }

    // 向心 + 限幅：每步最多挪 temperature 这么远，越到后面挪得越少
    const temperature = Math.max(TEMPERATURE * (1 - step / ITERATIONS), 1);
    for (let i = 0; i < count; i++) {
      dx[i] -= pos[i].x * GRAVITY;
      dy[i] -= pos[i].y * GRAVITY;
      const move = Math.hypot(dx[i], dy[i]);
      if (move < 0.01) continue;
      const limited = Math.min(move, temperature);
      pos[i].x += (dx[i] / move) * limited;
      pos[i].y += (dy[i] / move) * limited;
    }
  }

  // 质心归位：让整张图的中心落在画布原点上（见文件头的说明）
  const centerX = pos.reduce((sum, p) => sum + p.x, 0) / count;
  const centerY = pos.reduce((sum, p) => sum + p.y, 0) / count;

  const result = new Map<number, { x: number; y: number }>();
  nodes.forEach((node, i) => result.set(node.id, { x: pos[i].x - centerX, y: pos[i].y - centerY }));
  return result;
}

/**
 * 与节点矩形同宽高的椭圆，在 (ux, uy) 这个方向上的半径：
 * 正对着看是半宽、竖着看是半高，斜着取中间值。
 */
function ellipseRadius(ux: number, uy: number, halfWidth: number, halfHeight: number): number {
  return 1 / Math.hypot(ux / halfWidth, uy / halfHeight);
}
