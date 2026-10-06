#pragma once
#include "Env.h"

class Window;

/**
 * 后台线程干完活、要回包给网页时投这条消息：lParam 是一个 new 出来的 std::wstring（要发的 JSON），
 * 由窗口过程接住、发出去、再 delete。
 * 为什么要绕这一道：WebView2 的接口只能在建它的那个线程（UI 线程）上调，
 * 所以耗时的活（缩放图片）放到后台线程做，回包再投消息回 UI 线程发。
 */
constexpr UINT WM_DD_POST_JSON = WM_APP + 1;
class Page
{
public:
	Page(Window* win, ComPtr<ICoreWebView2>& webview);
	void emit(const JsonObject& eventData);
	/// 把已经拼好的 JSON 发给网页（供窗口过程在 UI 线程上转发后台线程的回包）
	void postJson(const std::wstring& json);
private:
	HRESULT onRequest(ICoreWebView2* webview, ICoreWebView2WebResourceRequestedEventArgs* args);
	HRESULT onMsgReceived(ICoreWebView2* webview, ICoreWebView2WebMessageReceivedEventArgs* args);
	HRESULT onDomLoaded(ICoreWebView2* sender, ICoreWebView2DOMContentLoadedEventArgs* args);
	HRESULT onRequestPermission(ICoreWebView2* webview, ICoreWebView2PermissionRequestedEventArgs* args);
	HRESULT onCloseWindow(ICoreWebView2* sender, IUnknown* args);
	std::wstring getContentType(const std::wstring& fileName);
	HRESULT serveFileFromDataPath(ICoreWebView2WebResourceRequestedEventArgs* args, const std::wstring& resName);
	void handleGetImageDir(JsonObject& result);
	/**
	 * args: { name, width, height, oldName? }；把 images 里 name 这张图按新的宽高另存一份（原图留着）。
	 * oldName 是正文里当前引用的那份：生成成功后把 image 表里指着它的记录改指到新的一份
	 * （见 Db/Image.h 的 renameReferences），再把同一张原图早先拖出来的其它尺寸都清掉
	 * （扫目录，见 Page.cpp 的 removeStaleResized），于是目录里只剩"原图 + 最后拖出来的那一份"。
	 * 还有文章在引用的那份留着不动——删了那边正文里的图就裂了。
	 *
	 * 回包是异步的：缩放放在后台线程做（大图要几百毫秒，在消息回调里做会把界面卡住），
	 * 做完投 WM_DD_POST_JSON 回 UI 线程再发。返回 { name }：新文件名（原主名 + @宽x高 + 原扩展名），
	 * 空串 = 没处理（格式 WIC 弄不了、读写失败、或 name 看着不像一个文件名），调用方继续用原图
	 */
	void handleResizeImage(const JsonObject& param);
private:
	Window* win;
	ComPtr<ICoreWebView2> webview;
};

