class Msg {
  private cache: Map<String, any> = new Map();
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
    if (msg.id && this.cache.has(msg.id)) {
      const pending = this.cache.get(msg.id);
      if (msg.error) {
        pending.reject(msg.error);
      } else {
        // withObjects 的请求额外把原生随回包附带的附加对象交出去（没有时给空数组）
        pending.resolve(pending.withObjects ? { result: msg.result, objects: event.additionalObjects ?? [] } : msg.result);
      }
      this.cache.delete(msg.id);
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
      this.cache.set(id, { resolve, reject, withObjects });
      /*@ts-ignore*/
      if (!window.chrome || !window.chrome.webview) {
        return;
      }
      /*@ts-ignore*/
      window.chrome.webview.postMessage(msg);
    });
  }
  on(eventName: String, listener: Function) {
    let arr = this.cache.get(eventName);
    if (arr) {
      arr.push(listener);
    } else {
      this.cache.set(eventName, [listener]);
    }
  }
  off(eventName: String, listener: Function) {
    let arr = this.cache.get(eventName);
    if (arr) {
      this.cache.set(
        eventName,
        arr.filter((item) => item != listener),
      );
    }
  }
  once(eventName: String, listener: Function) {
    const onceListener = (arg: any) => {
      listener(arg);
      this.off(eventName, onceListener);
    };
    this.on(eventName, onceListener);
  }
  emit(eventName: String, data?: any) {
    const listeners = this.cache.get(eventName);
    if (!listeners) return;
    for (const listener of [...listeners]) {
      listener(data);
    }
  }
}

export default new Msg();
