export default abstract class CtrlBase {
  protected readonly templateHtml: string;
  /** 模板插进文档后的根元素。appendTo() 之前没有值，之后必定非空，
   *  所以按非空类型对外声明，用时直接 this.dom.xxx，不必到处补 ! */
  dom!: HTMLElement;
  constructor(templateHtml: string) {
    this.templateHtml = templateHtml;
  }
  ready() {}
  /** 把模板插入 parent 末尾并调用 ready()；模板须为单一根元素，返回该根节点 */
  appendTo(parent: HTMLElement) {
    parent.insertAdjacentHTML("beforeend", this.templateHtml);
    this.dom = parent.lastElementChild as HTMLElement;
    this.ready();
  }
}
