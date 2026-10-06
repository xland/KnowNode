#pragma once
#include <filesystem>
#include <Windows.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <WebView2.h>
#include <wrl.h>
#include <dispatcherqueue.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.Data.Json.h>

using namespace Microsoft::WRL;
using namespace winrt::Windows::Data::Json;

class Window;
/**
 * 进程级初始化与服务定位：init() 里的调用顺序是有依赖的（数据目录 → 数据库 → WebView2 环境），别随便调换。
 * 这里保留类形态而不是改成 namespace，因为它是个有生命周期的对象：
 *   - 持有 WebView2 环境（ComPtr 自己会释放）；
 *   - WebView2 的环境创建回调以它为宿主（Callback<>(this, &Env::onEnvReady)），必须有 this。
 */
class Env
{
public:
	static void init();
	static std::filesystem::path getDataPath();
	static ICoreWebView2Environment* getWebViewEnv();
private:
	void checkRuntimeVersion();
	bool checkRegKey(const HKEY& key, const std::wstring& subKey);
	void initDataPath();
	void initWebViewEnv();
	static void initDispatcherQueueCtrl();
	HRESULT onEnvReady(HRESULT result, ICoreWebView2Environment* env);
private:
	std::filesystem::path dataPath; 
	ComPtr<ICoreWebView2Environment> webViewEnv;
};

