class Msg {
  /** 在飞的请求：id → 那一轮 invoke 的 resolve / reject。与事件监听器分开存，
   *  免得事件名恰好撞上某个 id（两者之前共用一个 Map）。 */
  private pending: Map<string, any> = new Map();
  /** 事件名 → 监听器数组（原生主动推送的事件，见 Arch/21-ipc.md） */
  private listeners: Map<string, Function[]> = new Map();

  constructor() {
    /*@ts-ignore*/
    if (!window.chrome || !window.chrome.webview) {
      return;
    }
    /*@ts-ignore*/
    window.chrome.webview.addEventListener("message", this.onMessage.bind(this));
  }

  private onMessage(event: any) {
    const msg = event.data;
    if (msg.id && this.pending.has(msg.id)) {
      const pending = this.pending.get(msg.id);
      if (msg.error) {
        pending.reject(msg.error);
      } else {
        // withObjects 的请求额外把原生随回包附带的附加对象交出去（没有时给空数组）
        pending.resolve(pending.withObjects ? { result: msg.result, objects: event.additionalObjects ?? [] } : msg.result);
      }
      this.pending.delete(msg.id);
    } else if (msg.eventName) {
      this.emit(msg.eventName, msg);
    }
  }

  invoke(method: String, args?: any): Promise<unknown> {
    return this.post(method, args, false);
  }

  /**
   * 与 invoke 同构，但回包 resolve 的是 { result, objects }：
   * objects 是原生用 PostWebMessageAsJsonWithAdditionalObjects 随回包附带的对象数组
   * （如 File System Access 的目录句柄），原生没附带对象时是空数组。
   */
  invokeWithObjects(method: String, args?: any): Promise<{ result: any; objects: any[] }> {
    return this.post(method, args, true) as Promise<{ result: any; objects: any[] }>;
  }

  private post(method: String, args: any, withObjects: boolean): Promise<unknown> {
    return new Promise((resolve, reject) => {
      const id = Math.random().toString(8).substring(2);
      const msg = { id, method, args };
      this.pending.set(id, { resolve, reject, withObjects });
      /*@ts-ignore*/
      if (!window.chrome || !window.chrome.webview) {
        return;
      }
      /*@ts-ignore*/
      window.chrome.webview.postMessage(msg);
    });
  }

  on(eventName: String, listener: Function) {
    const key = String(eventName);
    const arr = this.listeners.get(key);
    if (arr) {
      arr.push(listener);
    } else {
      this.listeners.set(key, [listener]);
    }
  }

  off(eventName: String, listener: Function) {
    const key = String(eventName);
    const arr = this.listeners.get(key);
    if (!arr) return;
    this.listeners.set(
      key,
      arr.filter((item) => item != listener),
    );
  }

  once(eventName: String, listener: Function) {
    const onceListener = (arg: any) => {
      listener(arg);
      this.off(eventName, onceListener);
    };
    this.on(eventName, onceListener);
  }

  emit(eventName: String, data?: any) {
    const listeners = this.listeners.get(String(eventName));
    if (!listeners) return;
    for (const listener of [...listeners]) {
      listener(data);
    }
  }
}

export default new Msg();
