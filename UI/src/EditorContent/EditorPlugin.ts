import Msg from "../Msg";
import { getFormatState } from "roosterjs-content-model-api";
import type { ContentModelFormatState, IEditor, PluginEvent } from "roosterjs-content-model-types";

export default class EditorPlugin {
  private editor!: IEditor;
  private lastState: ContentModelFormatState | null = null;
  private timer = 0;
  /** 编辑操作后延迟多久刷新按钮状态（毫秒），对应官方 createRibbonPlugin 的 delayUpdateTime */
  private delayUpdateTime: number;

  /**
   * @param delayUpdateTime 用户编辑操作后延迟多久刷新按钮状态（毫秒），默认 200，与官方一致
   */
  constructor(delayUpdateTime = 200) {
    this.delayUpdateTime = delayUpdateTime;
  }

  getName(): string {
    return "EditorState";
  }

  initialize(editor: IEditor): void {
    this.editor = editor;
  }

  dispose(): void {
    // 取消尚未触发的延迟刷新，避免销毁后还去读已经失效的 editor
    if (this.timer) {
      this.editor.getDocument().defaultView?.clearTimeout(this.timer);
      this.timer = 0;
    }
  }

  /**
   * 事件分支与官方 createRibbonPlugin 完全一致：
   * - editorReady / contentChanged / zoomChanged → 立即刷新（频次低、语义明确）
   * - keyDown / mouseUp → 走 delayUpdate 防抖（高频事件，合并刷新）
   * 其余事件不触发刷新。
   */
  onPluginEvent(event: PluginEvent): void {
    switch (event.eventType) {
      case "editorReady":
      case "zoomChanged":
        this.emitState();
        break;

      case "contentChanged":
        this.emitState();
        // 正文有变化：由 ArticleTitle 排队写回当前选中的那篇（最多 2 秒一次）
        Msg.emit("editorContentChanged");
        break;

      // 普通打字要单独接：光标折叠时 rooster 让浏览器直接把字符写进 DOM，
      // 只作废模型缓存、并不发 contentChanged（只有删除/回车/粘贴/格式 API 这类
      // 走 formatContentModel 的操作才发）。不接这两个事件，打的字根本不会被保存。
      // 输入法上屏发的是 compositionEnd，也要算。
      case "input":
      case "compositionEnd":
        Msg.emit("editorContentChanged");
        break;

      case "keyDown":
      case "mouseUp":
        this.delayUpdate();
        break;
    }
  }

  /** 官方 delayUpdate：尾触发防抖，定时器挂在编辑器所在的 window 上 */
  private delayUpdate(): void {
    const win = this.editor.getDocument().defaultView!;

    if (this.timer) {
      win.clearTimeout(this.timer);
    }

    this.timer = win.setTimeout(() => {
      this.timer = 0;
      this.emitState();
    }, this.delayUpdateTime);
  }

  /**
   * 广播工具栏状态，与官方 createRibbonPlugin 完全同款：
   * 数据源只有 getFormatState(editor)，键数量 + 逐键比较，状态没变化就不广播，
   * 避免每次按键都让所有按钮重算。官方也不在 formatState 之外附加任何选区信息。
   */
  private emitState(): void {
    const next: ContentModelFormatState = getFormatState(this.editor);
    const prev = this.lastState;
    if (prev) {
      const keys = Object.keys(next) as (keyof ContentModelFormatState)[];
      const unchanged = keys.length === Object.keys(prev).length && keys.every((key) => Object.is(next[key], prev[key]));
      if (unchanged) {
        return;
      }
    }
    this.lastState = next;
    Msg.emit("editorState", next);
  }
}
