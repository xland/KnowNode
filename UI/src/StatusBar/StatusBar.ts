import "./StatusBar.scss";
import html from "./StatusBar.html?raw";
import CtrlBase from "../CtrlBase";
import Msg from "../Msg";
import EditorContent from "../EditorContent/EditorContent";

/**
 * 正文字数：HTML 剥掉标签后的字符数。
 * 借 DOMParser 走一遍解析，而不是拿正则去标签：实体（&nbsp; &amp; &lt;…）由它解成真正的字符，
 * 一个汉字算一个字符；用展开运算符按码点数，emoji 这类代理对也不会被算成两个。
 */
function countChars(html: string): number {
  const doc = new DOMParser().parseFromString(html, "text/html");
  return [...(doc.body.textContent ?? "")].length;
}

/**
 * 底部状态栏（模块单例）。由 Main 挂在 body 最下面。
 * 左侧是库里的分类数与文章总数（总数，不随当前选中哪个分类变化）；右侧是当前文章的字数。
 * 字数跟的是编辑器里的实时正文：EditorPlugin 一广播 editorContentChanged 就重算，
 * 所以打字、粘贴、打开另一篇都立刻跟上，不用等那 2 秒的入库防抖。
 */
class StatusBar extends CtrlBase {
  /** 左侧：库级统计 */
  private counts: HTMLElement | null = null;

  /** 右侧：当前文章字数 */
  private words: HTMLElement | null = null;

  constructor() {
    super(html);
  }

  override ready(): void {
    this.counts = this.dom.querySelector<HTMLElement>("#statusCounts");
    this.words = this.dom.querySelector<HTMLElement>("#statusWords");
    Msg.on("editorContentChanged", () => this.updateWords());
    void this.refreshCounts();
    this.updateWords();
  }

  /**
   * 左侧统计：向原生要一次库里的分类数与文章数（一个 getCounts）。
   * 由 Category（分类增删后）和 ArticleTitle（文章增删后）在重画完调一次——
   * 挂在"重画"上而不是挂在"写库"上，就不用在每个改动入口都记得通知状态栏。
   */
  async refreshCounts(): Promise<void> {
    if (!this.counts) return;
    try {
      const data = (await Msg.invoke("getCounts")) as { categories?: number; articles?: number };
      this.counts.textContent = `分类：共 ${data?.categories ?? 0} 个，文章：共 ${data?.articles ?? 0} 篇`;
    } catch {
      this.counts.textContent = ""; // 取不到就空着：不写一个 0 上去冒充真实数字
    }
  }

  /**
   * 右侧字数；编辑器还没建好（启动瞬间、还没打开任何文章）时留空。
   * public：正文被程序化改动（切到空分类、删掉最后一篇后清空编辑器）时不会走用户编辑那条事件链，
   * 由 ArticleTitle 直接调一次。
   */
  updateWords(): void {
    if (!this.words) return;
    this.words.textContent = EditorContent.editor ? `当前文章共 ${countChars(EditorContent.content)} 个字` : "";
  }
}

export default new StatusBar();
