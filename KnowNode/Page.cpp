#include "Env.h"
#include "Page.h"
#include "Window.h"
#include "Db/Db.h"
#include "Util.h"

#include <fstream>
#include <filesystem>
#include <thread>

Page::Page(Window* win, ComPtr<ICoreWebView2>& webview) :win{ win }, webview{ webview }
{
    webview->AddWebResourceRequestedFilter(L"https://app.localhost/*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    auto resRequestedCB = Callback<ICoreWebView2WebResourceRequestedEventHandler>(this, &Page::onRequest);
    webview->add_WebResourceRequested(resRequestedCB.Get(), nullptr);

    auto msgReceivedCB = Callback<ICoreWebView2WebMessageReceivedEventHandler>(this, &Page::onMsgReceived);
    webview->add_WebMessageReceived(msgReceivedCB.Get(), nullptr);

    auto reqPermissionCB = Callback<ICoreWebView2PermissionRequestedEventHandler>(this, &Page::onRequestPermission);
    webview->add_PermissionRequested(reqPermissionCB.Get(), nullptr);

    ComPtr<ICoreWebView2_2> webview2;
    this->webview.As(&webview2);
    auto domLoadedCB = Callback<ICoreWebView2DOMContentLoadedEventHandler>(this, &Page::onDomLoaded);
    webview2->add_DOMContentLoaded(domLoadedCB.Get(), nullptr);

    auto closeWindowCB = Callback<ICoreWebView2WindowCloseRequestedEventHandler>(this, &Page::onCloseWindow);
    webview->add_WindowCloseRequested(closeWindowCB.Get(), nullptr);

#ifdef _DEBUG
	// 调试：用 vite 开发服务器，改前端不必重新编译 exe
	webview->Navigate(L"http://localhost:5173");
	webview->OpenDevToolsWindow();
#else
	// 发布：前端产物编进了 exe 资源（见 Resource.rc 里那块 dist 清单），走虚拟域名——
	// 请求由 onRequest 从资源里应答，一个 exe 就能独立跑，不依赖本机任何文件
	webview->Navigate(L"https://app.localhost/index.html");
#endif
}

void Page::emit(const JsonObject& eventData)
{
    std::wstring eventDataStr{ eventData.Stringify() };
    webview->PostWebMessageAsJson(eventDataStr.data());
}


HRESULT Page::onMsgReceived(ICoreWebView2* webview, ICoreWebView2WebMessageReceivedEventArgs* args)
{
    PWSTR jsonRaw;
    auto hr = args->get_WebMessageAsJson(&jsonRaw);
    if (FAILED(hr)) return S_OK;
    JsonObject param = JsonObject::Parse(jsonRaw);
    CoTaskMemFree(jsonRaw);
    auto method = param.GetNamedString(L"method");
    JsonObject result;
    result.SetNamedValue(L"id", JsonValue::CreateStringValue(param.GetNamedString(L"id")));
    if (method == L"showWindow") {
        win->show();
    }
    else if (method == L"hittest") {
        // args: { val }；val 是 HT_* 命中值，由前端 WindowBorder 给
        win->hittest(static_cast<int>(param.GetNamedObject(L"args").GetNamedNumber(L"val")));
    }
    else if (method == L"minimize") {
        win->minimize();
    }
    else if (method == L"maximize") {
        win->maximize();
    }
    else if (method == L"restore") {
        win->restore();
    }
    else if (method == L"getImageDir") {
        // 把图片目录句柄随回包发给 JS（自带回包逻辑，不走下面的统一 PostWebMessageAsJson）
        handleGetImageDir(result);
        return S_OK;
    }    
    else {
        // 未知方法回一个 error：前端 Msg.invoke 会 reject，而不是静默 resolve(undefined)。
        // 之前"原生侧改了却忘了重新编译 exe"就是被静默吞掉的，补上这条能直接暴露出来
        std::wstring message = L"unknown method: " + std::wstring(method.c_str());
        result.SetNamedValue(L"error", JsonValue::CreateStringValue(message));
    }
    auto resultStr = result.Stringify();
    webview->PostWebMessageAsJson(resultStr.data());
    return S_OK;
}

HRESULT Page::onCloseWindow(ICoreWebView2* sender, IUnknown* args)
{
    PostMessage(win->hwnd, WM_CLOSE, 0, 0);
    return S_OK;
}

HRESULT Page::onDomLoaded(ICoreWebView2* sender, ICoreWebView2DOMContentLoadedEventArgs* args)
{
    return S_OK;
}

HRESULT Page::onRequestPermission(ICoreWebView2* webview, ICoreWebView2PermissionRequestedEventArgs* args)
{
    args->put_State(COREWEBVIEW2_PERMISSION_STATE_ALLOW);
    return S_OK;
}


HRESULT Page::onRequest(ICoreWebView2* webview, ICoreWebView2WebResourceRequestedEventArgs* args)
{
    ComPtr<ICoreWebView2WebResourceRequest> request;
    args->get_Request(&request);
    LPWSTR rawUri = nullptr;
    request->get_Uri(&rawUri);
    std::wstring url(rawUri);
    CoTaskMemFree(rawUri);
    size_t queryPos = url.find(L'?');
    size_t end = (queryPos != std::wstring::npos) ? queryPos : url.length();
    std::wstring resName = url.substr(22, end - 22); //22是“https://app.localhost/”的长度
    // 光有域名（https://app.localhost/）或以 / 结尾的，都当要首页
    if (resName.empty() || resName.back() == L'/') resName += L"index.html";
    HRSRC hRes = FindResource(NULL, resName.data(), RT_RCDATA);
    if (!hRes) {
        // 内嵌资源未命中时，回退到数据目录：读取 dataPath/<resName> 同名文件
        auto hr = serveFileFromDataPath(args, resName);
        // 文件也不存在时保持默认请求失败行为（不 put_Response，让上层按 404 处理）
        return hr == S_OK ? S_OK : hr;
    }
    HGLOBAL hData = LoadResource(NULL, hRes);
    if (!hData) return S_OK;
    void* pData = LockResource(hData);
    DWORD size = SizeofResource(NULL, hRes);
    ComPtr<IStream> stream = SHCreateMemStream((const BYTE*)pData, size);
    auto ct = getContentType(resName);
    ComPtr<ICoreWebView2WebResourceResponse> response;
    Env::getWebViewEnv()->CreateWebResourceResponse(stream.Get(), 200, L"OK", ct.data(), &response);
    args->put_Response(response.Get());
    return S_OK;
}

std::wstring Page::getContentType(const std::wstring& fileName)
{
    static const std::unordered_map<std::string, std::wstring> mimeTypes = {
        {".html", L"Content-Type: text/html"},
        {".htm",  L"Content-Type: text/html"},
        {".js",   L"Content-Type: application/javascript"},
        {".css",  L"Content-Type: text/css"},
        {".json", L"Content-Type: application/json"},
        {".png",  L"Content-Type: image/png"},
        {".jpg",  L"Content-Type: image/jpeg"},
        {".jpeg", L"Content-Type: image/jpeg"},
        {".gif",  L"Content-Type: image/gif"},
        {".svg",  L"Content-Type: image/svg+xml"},
        {".ico",  L"Content-Type: image/x-icon"},
        {".woff", L"Content-Type: font/woff"},
        {".woff2",L"Content-Type: font/woff2"},
        {".ttf",  L"Content-Type: font/ttf"},
        {".eot",  L"Content-Type: application/vnd.ms-fontobject"},
        {".txt",  L"Content-Type: text/plain"},
        {".wasm", L"Content-Type: application/wasm"},
        {".mp3",  L"Content-Type: audio/mpeg"},
        {".mp4",  L"Content-Type: video/mp4"}
    };
    std::filesystem::path path(fileName);
    auto ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    auto it = mimeTypes.find(ext);
    if (it != mimeTypes.end()) {
        return it->second;
    }
    return L"Content-Type: application/octet-stream";
}

HRESULT Page::serveFileFromDataPath(ICoreWebView2WebResourceRequestedEventArgs* args, const std::wstring& resName)
{
    // resName 直接来自 URL（可含子目录，如 images/xxx.png）：规范化后必须仍在数据目录内，
    // 挡掉 ../ 之类跳出目录的请求，同时保留了对 images 子目录的支持
    if (resName.empty()) return S_FALSE;
    auto base = Env::getDataPath().lexically_normal();
    auto filePath = (base / resName).lexically_normal();
    std::error_code ec;
    auto rel = std::filesystem::relative(filePath, base, ec).wstring();
    if (ec || rel.empty() || rel == L".." || rel.rfind(L"..\\", 0) == 0 || rel.rfind(L"../", 0) == 0) {
        return S_FALSE;
    }

    auto sz = std::filesystem::file_size(filePath, ec);
    if (ec) return S_FALSE; // 文件不存在，交由调用方决定
    std::ifstream f(filePath, std::ios::binary);
    std::string bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (bytes.empty()) return S_FALSE;
    ComPtr<IStream> stream = SHCreateMemStream((const BYTE*)bytes.data(), (UINT)bytes.size());
    auto ct = getContentType(resName);
    ComPtr<ICoreWebView2WebResourceResponse> response;
    Env::getWebViewEnv()->CreateWebResourceResponse(stream.Get(), 200, L"OK", ct.data(), &response);
    args->put_Response(response.Get());
    return S_OK;
}

void Page::handleGetImageDir(JsonObject& result)
{
    // 只把图片子目录交给 JS：数据目录里还放着 SQLite 库和 WebView2 的用户数据，
    // 整个目录给 READ_WRITE 等于让前端能读写甚至删掉数据库
    std::error_code ec;
    auto dir = Env::getDataPath() / L"images";
    std::filesystem::create_directories(dir, ec);

    ComPtr<ICoreWebView2Environment14> env14;
    ComPtr<ICoreWebView2FileSystemHandle> dirHandle;
    ComPtr<ICoreWebView2_23> webview23;
    if (!ec
        && SUCCEEDED(Env::getWebViewEnv()->QueryInterface(IID_PPV_ARGS(&env14)))
        && SUCCEEDED(env14->CreateWebFileSystemDirectoryHandle(dir.c_str(),
            COREWEBVIEW2_FILE_SYSTEM_HANDLE_PERMISSION_READ_WRITE, &dirHandle))
        && SUCCEEDED(webview->QueryInterface(IID_PPV_ARGS(&webview23))))
    {
        IUnknown* items[] = { dirHandle.Get() };
        ComPtr<ICoreWebView2ObjectCollection> collection;
        if (SUCCEEDED(env14->CreateObjectCollection(1, items, &collection)))
        {
            // 自带回包：句柄只能随附加对象一起发，成功后直接返回
            auto json = result.Stringify();
            webview23->PostWebMessageAsJsonWithAdditionalObjects(json.c_str(), collection.Get());
            return;
        }
    }
    result.SetNamedValue(L"error", JsonValue::CreateStringValue(L"获取图片目录失败"));
    auto json = result.Stringify();
    webview->PostWebMessageAsJson(json.c_str());
}

void Page::postJson(const std::wstring& json)
{
    webview->PostWebMessageAsJson(json.c_str());
}

namespace
{
    /**
     * 这个图片文件还有没有文章在引用：数 image 表里 is_delete = 0 的记录（见 Db/Image.h）。
     *
     * 注意记录是滞后的：正文拖完还没入库时，库里那行写的仍是旧文件。所以调用方要先调
     * Image::renameReferences 把旧文件的记录改指到新产物上，再来问这一句——否则当前这篇
     * 自己的滞后记录会把要清理的文件一直保着。
     * 查不到（库没开）时按"有人引用"处理：宁可留一份垃圾文件，也别把别处正文里的图删成裂图
     */
    bool stillReferenced(const std::wstring& imageName)
    {
        sqlite3* conn = Db::get();
        if (!conn) return true;
        int count = 0;
        static const char* sql = "SELECT COUNT(*) FROM image WHERE img_name = ?1 AND is_delete = 0;";
        if (sqlite3_stmt* stmt = nullptr; sqlite3_prepare_v2(conn, sql, -1, &stmt, nullptr) == SQLITE_OK)
        {
            sqlite3_bind_text16(stmt, 1, imageName.c_str(), -1, SQLITE_TRANSIENT);
            if (sqlite3_step(stmt) == SQLITE_ROW) count = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
        }
        return count > 0;
    }

    /**
     * 清掉同一张原图早先拖出来的其它尺寸（img_x@600x400.png 这类），只留这一次生成的那一份。
     *
     * 为什么扫目录而不是只删前端报上来的那一份：连着拖几下、或中途生成的尺寸，前端报不全，
     * 目录里就攒下没人认领的旧文件。产物名是确定性的（原主名@宽x高+原扩展名），扫一遍就认得出：
     * 原图自己不带 @，别的图主名不同，都落不进这个范围。
     */
    void removeStaleResized(const std::filesystem::path& dir, const std::wstring& stem, const std::wstring& ext, const std::wstring& keep)
    {
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec))
        {
            if (!entry.is_regular_file()) continue;
            auto fileName = entry.path().filename().wstring();
            if (fileName == keep) continue; // 这一次要用的这份：留着
            if (fileName.rfind(stem + L"@", 0) != 0) continue;
            if (fileName.size() < stem.size() + 1 + ext.size()) continue;
            if (fileName.compare(fileName.size() - ext.size(), ext.size(), ext) != 0) continue;
            // 还有文章引用就留着：删了那边正文里的图就裂了
            if (stillReferenced(fileName)) continue;
            std::filesystem::remove(entry.path(), ec);
        }
    }
}