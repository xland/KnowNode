/**
 * 力导向布局：只算坐标，不碰 Konva（Arch/32 要求布局与绘制解耦）。
 *
 * 用法是**一次性**的：点「一键整理」时从当前坐标出发算到平衡，算出新坐标交给画布去做动画，
 * 不像 d3-force 那样持续演化。因此没有 alpha / reheat / 逐帧推进那一套，
 * 收敛判据就是"迭代次数跑完"。
 *
 * 算法是 Fruchterman-Reingold 的变体，改了两处：
 * - 斥力用**两个节点表面之间的距离**（中心距减去各自的外接圆半径）而不是中心距，
 *   节点越大越难被挤到一起，矩形尺寸的差别也就体现得出来；
 * - 加了一个很弱的**向心力**把整张图拉回原点，免得互不连通的那几块各自飘走。
 *
 * 权重已删除（Arch/32），所以每条边的弹簧长度都一样：`IDEAL_DISTANCE`。
 */

export interface LayoutNode {
  id: number;
  x: number;
  y: number;
  /** 节点的外接圆半径（由矩形宽高算出）：斥力按它算，防重叠 */
  radius: number;
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
/** 斥力里两个节点之间至少留出的空隙（在半径之和以外再留这么多） */
const PADDING = 24;
/** 向心力：只防飘散，不主导布局 */
const GRAVITY = 0.02;

/**
 * 算出力导向平衡后的坐标，返回「节点 id → 新坐标」。
 * 从**当前坐标**出发（用户已经摆过一遍，从这里收敛比从随机位置快，也不会无故翻个面）。
 */
export function forceLayout(nodes: LayoutNode[], lines: LayoutLine[]): Map<number, { x: number; y: number }> {
  const count = nodes.length;
  const pos = nodes.map((node) => ({ x: node.x, y: node.y }));
  const radius = nodes.map((node) => node.radius);

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
        // 表面之间还剩多少空隙：贴上了（gap 很小）斥力就猛涨，把两个节点推开
        const gap = Math.max(distance - radius[i] - radius[j] - PADDING, 1);
        const force = (IDEAL_DISTANCE * IDEAL_DISTANCE) / (gap * gap);
        const ux = vx / distance;
        const uy = vy / distance;
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

  const result = new Map<number, { x: number; y: number }>();
  nodes.forEach((node, i) => result.set(node.id, { x: pos[i].x, y: pos[i].y }));
  return result;
}
