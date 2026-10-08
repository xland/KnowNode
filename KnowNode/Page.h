#pragma once
#include "Env.h"

class Window;

class Page
{
public:
	Page(Window* win, ComPtr<ICoreWebView2>& webview);
	/// 主动推一个事件给网页（如窗口最大化 / 还原），前端用 Msg.on 订阅
	void emit(const JsonObject& eventData);
	/// method 注册表里的 handler 要用到的两个东西：窗口控制、以及自带回包的图片目录
	Window* window() const { return win; }
	void handleGetImageDir(JsonObject& result);
private:
	HRESULT onRequest(ICoreWebView2* webview, ICoreWebView2WebResourceRequestedEventArgs* args);
	HRESULT onMsgReceived(ICoreWebView2* webview, ICoreWebView2WebMessageReceivedEventArgs* args);
	HRESULT onRequestPermission(ICoreWebView2* webview, ICoreWebView2PermissionRequestedEventArgs* args);
	HRESULT onCloseWindow(ICoreWebView2* sender, IUnknown* args);
	std::wstring getContentType(const std::wstring& fileName);
	HRESULT serveFileFromDataPath(ICoreWebView2WebResourceRequestedEventArgs* args, const std::wstring& resName);

	Window* win;
	ComPtr<ICoreWebView2> webview;
};

