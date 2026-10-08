#include "Env.h"
#include "Page.h"
#include "Window.h"
#include "Db/KnowDetail.h"
#include "Db/KnowLine.h"
#include "Db/KnowList.h"
#include "Db/KnowNode.h"
#include "Db/Setting.h"
#include "Util.h"

#include <fstream>
#include <filesystem>
#include <functional>
#include <map>

namespace
{
    /// 取 args 里的浮点参数（节点坐标）；缺失或类型不对时按 0 处理
    double argDouble(const JsonObject& args, const wchar_t* key)
    {
        if (!args.HasKey(key)) return 0;
        auto value = args.GetNamedValue(key);
        return value.ValueType() == JsonValueType::Number ? value.GetNumber() : 0;
    }

    std::wstring toWide(const std::string& utf8) { return Util::convertToWStr(utf8.c_str()); }
    std::string toUtf8(const std::wstring& wide) { return Util::convertToStr(wide.c_str()); }

    JsonValue num(double v) { return JsonValue::CreateNumberValue(v); }
    JsonValue text(const std::string& v) { return JsonValue::CreateStringValue(toWide(v)); }
    void fail(JsonObject& result, const wchar_t* message)
    {
        result.SetNamedValue(L"error", JsonValue::CreateStringValue(message));
    }

    void listJson(JsonArray& arr, const KnowListItem& item)
    {
        JsonObject o;
        o.SetNamedValue(L"id", num(static_cast<double>(item.id)));
        o.SetNamedValue(L"name", text(item.name));
        o.SetNamedValue(L"detailId", num(static_cast<double>(item.detailId)));
        arr.Append(o);
    }
    void nodeJson(JsonArray& arr, const KnowNodeItem& item)
    {
        JsonObject o;
        o.SetNamedValue(L"id", num(static_cast<double>(item.id)));
        o.SetNamedValue(L"listId", num(static_cast<double>(item.listId)));
        o.SetNamedValue(L"title", text(item.title));
        o.SetNamedValue(L"detailId", num(static_cast<double>(item.detailId)));
        o.SetNamedValue(L"x", num(item.x));
        o.SetNamedValue(L"y", num(item.y));
        o.SetNamedValue(L"color", num(item.color));
        arr.Append(o);
    }
    void lineJson(JsonArray& arr, const KnowLineItem& item)
    {
        JsonObject o;
        o.SetNamedValue(L"id", num(static_cast<double>(item.id)));
        o.SetNamedValue(L"nodeAId", num(static_cast<double>(item.nodeAId)));
        o.SetNamedValue(L"nodeBId", num(static_cast<double>(item.nodeBId)));
        o.SetNamedValue(L"detailId", num(static_cast<double>(item.detailId)));
        o.SetNamedValue(L"color", num(item.color));
        arr.Append(o);
    }

    /// 知识 / 节点 / 连线的详情 id：三者各自持有 detail_id，这里按 target 分派
    int64_t detailIdOf(const std::wstring& target, int64_t id)
    {
        if (target == L"list")
        {
            KnowListItem item;
            return KnowList::instance().get(id, item) ? item.detailId : 0;
        }
        if (target == L"node")
        {
            KnowNodeItem item;
            return KnowNode::instance().get(id, item) ? item.detailId : 0;
        }
        if (target == L"line")
        {
            KnowLineItem item;
            return KnowLine::instance().get(id, item) ? item.detailId : 0;
        }
        return 0;
    }

    /**
     * method → handler 注册表（命名与清单见 Arch/21-ipc.md）。
     * handler 返回 true 表示**它自己已经发过回包**（image.dir 要随包附带目录句柄），
     * 调用方不要再发一次。统一回包、未知 method 回 error 的行为都在 onMsgReceived 里。
     */
    const std::map<std::wstring, std::function<bool(Page*, const JsonObject&, JsonObject&)>>& msgHandlers()
    {
        using Handler = std::function<bool(Page*, const JsonObject&, JsonObject&)>;
        static const std::map<std::wstring, Handler> table = {
            // ---- 窗口控制 ----
            { L"win.show", [](Page* p, const JsonObject&, JsonObject&) { p->window()->show(); return false; } },
            { L"win.hittest", [](Page* p, const JsonObject& a, JsonObject&) {
                p->window()->hittest(static_cast<int>(Util::argNumber(a, L"val"))); return false; } },
            { L"win.minimize", [](Page* p, const JsonObject&, JsonObject&) { p->window()->minimize(); return false; } },
            { L"win.maximize", [](Page* p, const JsonObject&, JsonObject&) { p->window()->maximize(); return false; } },
            { L"win.restore", [](Page* p, const JsonObject&, JsonObject&) { p->window()->restore(); return false; } },

            // ---- 知识 list ----
            { L"list.list", [](Page*, const JsonObject&, JsonObject& r) {
                JsonArray arr;
                for (const auto& item : KnowList::instance().all()) listJson(arr, item);
                r.SetNamedValue(L"result", arr);
                return false; } },
            { L"list.create", [](Page*, const JsonObject& a, JsonObject& r) {
                auto name = toUtf8(Util::argString(a, L"name"));
                if (name.empty()) { fail(r, L"知识名称不能为空"); return false; }
                auto id = KnowList::instance().add(name);
                if (id == 0) { fail(r, L"新建知识失败"); return false; }
                r.SetNamedValue(L"result", num(static_cast<double>(id)));
                return false; } },
            { L"list.update", [](Page*, const JsonObject& a, JsonObject& r) {
                auto name = toUtf8(Util::argString(a, L"name"));
                if (name.empty()) { fail(r, L"知识名称不能为空"); return false; }
                if (!KnowList::instance().rename(Util::argNumber(a, L"id"), name)) { fail(r, L"重命名知识失败"); return false; }
                return false; } },
            { L"list.remove", [](Page*, const JsonObject& a, JsonObject& r) {
                if (!KnowList::instance().remove(Util::argNumber(a, L"id"))) { fail(r, L"删除知识失败"); return false; }
                return false; } },
            // 聚合：点列表项时一次取回该知识的节点 + 连线
            { L"list.open", [](Page*, const JsonObject& a, JsonObject& r) {
                auto listId = Util::argNumber(a, L"id");
                JsonArray nodes;
                for (const auto& item : KnowNode::instance().ofList(listId)) nodeJson(nodes, item);
                JsonArray lines;
                for (const auto& item : KnowLine::instance().ofList(listId)) lineJson(lines, item);
                JsonObject data;
                data.SetNamedValue(L"nodes", nodes);
                data.SetNamedValue(L"lines", lines);
                r.SetNamedValue(L"result", data);
                return false; } },

            // ---- 节点 node ----
            { L"node.list", [](Page*, const JsonObject& a, JsonObject& r) {
                JsonArray arr;
                for (const auto& item : KnowNode::instance().ofList(Util::argNumber(a, L"listId"))) nodeJson(arr, item);
                r.SetNamedValue(L"result", arr);
                return false; } },
            { L"node.create", [](Page*, const JsonObject& a, JsonObject& r) {
                auto title = toUtf8(Util::argString(a, L"title"));
                auto id = title.empty()
                    ? KnowNode::instance().add(Util::argNumber(a, L"listId"), argDouble(a, L"x"), argDouble(a, L"y"))
                    : KnowNode::instance().add(Util::argNumber(a, L"listId"), argDouble(a, L"x"), argDouble(a, L"y"), title);
                if (id == 0) { fail(r, L"新建节点失败"); return false; }
                r.SetNamedValue(L"result", num(static_cast<double>(id)));
                return false; } },
            { L"node.update", [](Page*, const JsonObject& a, JsonObject& r) {
                // 标题为空时回落到表类里的默认值「未命名」，不在这里另立一份规则
                if (!KnowNode::instance().updateTitle(Util::argNumber(a, L"id"), toUtf8(Util::argString(a, L"title"))))
                { fail(r, L"修改节点标题失败"); return false; }
                return false; } },
            { L"node.move", [](Page*, const JsonObject& a, JsonObject& r) {
                if (!KnowNode::instance().move(Util::argNumber(a, L"id"), argDouble(a, L"x"), argDouble(a, L"y")))
                { fail(r, L"保存节点坐标失败"); return false; }
                return false; } },
            { L"node.remove", [](Page*, const JsonObject& a, JsonObject& r) {
                if (!KnowNode::instance().remove(Util::argNumber(a, L"id"))) { fail(r, L"删除节点失败"); return false; }
                return false; } },
            // 标记色：0 = 未着色，1..6 = 具体颜色；越界的值由表类夹到边界上
            { L"node.color", [](Page*, const JsonObject& a, JsonObject& r) {
                if (!KnowNode::instance().setColor(Util::argNumber(a, L"id"),
                        static_cast<int>(Util::argNumber(a, L"color"))))
                { fail(r, L"设置节点颜色失败"); return false; }
                return false; } },

            // ---- 连线 line ----
            { L"line.list", [](Page*, const JsonObject& a, JsonObject& r) {
                JsonArray arr;
                for (const auto& item : KnowLine::instance().ofList(Util::argNumber(a, L"listId"))) lineJson(arr, item);
                r.SetNamedValue(L"result", arr);
                return false; } },
            { L"line.create", [](Page*, const JsonObject& a, JsonObject& r) {
                auto id = KnowLine::instance().add(Util::argNumber(a, L"nodeAId"), Util::argNumber(a, L"nodeBId"));
                if (id == 0) { fail(r, L"建立关联失败：两端必须属于同一个知识，且不能是同一个节点"); return false; }
                r.SetNamedValue(L"result", num(static_cast<double>(id)));
                return false; } },
            { L"line.remove", [](Page*, const JsonObject& a, JsonObject& r) {
                if (!KnowLine::instance().remove(Util::argNumber(a, L"id"))) { fail(r, L"删除关联失败"); return false; }
                return false; } },
            { L"line.color", [](Page*, const JsonObject& a, JsonObject& r) {
                if (!KnowLine::instance().setColor(Util::argNumber(a, L"id"),
                        static_cast<int>(Util::argNumber(a, L"color"))))
                { fail(r, L"设置关联颜色失败"); return false; }
                return false; } },

            // ---- 详情 detail ----
            { L"detail.get", [](Page*, const JsonObject& a, JsonObject& r) {
                auto target = Util::argString(a, L"target");
                if (target != L"list" && target != L"node" && target != L"line") { fail(r, L"target 只能是 list、node 或 line"); return false; }
                r.SetNamedValue(L"result", text(KnowDetail::instance().content(detailIdOf(target, Util::argNumber(a, L"id")))));
                return false; } },
            { L"detail.save", [](Page*, const JsonObject& a, JsonObject& r) {
                auto target = Util::argString(a, L"target");
                if (target != L"list" && target != L"node" && target != L"line") { fail(r, L"target 只能是 list、node 或 line"); return false; }
                if (!KnowDetail::instance().save(detailIdOf(target, Util::argNumber(a, L"id")),
                        toUtf8(Util::argString(a, L"content"))))
                { fail(r, L"保存详情失败"); return false; }
                return false; } },

            // ---- 设置 setting ----
            { L"setting.get", [](Page*, const JsonObject& a, JsonObject& r) {
                auto fallback = toUtf8(Util::argString(a, L"fallback"));
                r.SetNamedValue(L"result", text(Setting::instance().get(toUtf8(Util::argString(a, L"key")), fallback)));
                return false; } },
            { L"setting.set", [](Page*, const JsonObject& a, JsonObject& r) {
                Setting::instance().set(toUtf8(Util::argString(a, L"key")), toUtf8(Util::argString(a, L"value")));
                return false; } },

            // ---- 图片 image（沿用老项目逻辑，只改了 method 名）----
            { L"image.dir", [](Page* p, const JsonObject&, JsonObject& r) {
                // 句柄只能随附加对象一起发，成功后 handler 内部已回包
                p->handleGetImageDir(r);
                return true; } },
        };
        return table;
    }
}

Page::Page(Window* win, ComPtr<ICoreWebView2>& webview) :win{ win }, webview{ webview }
{
    webview->AddWebResourceRequestedFilter(L"https://app.localhost/*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
    auto resRequestedCB = Callback<ICoreWebView2WebResourceRequestedEventHandler>(this, &Page::onRequest);
    webview->add_WebResourceRequested(resRequestedCB.Get(), nullptr);

    auto msgReceivedCB = Callback<ICoreWebView2WebMessageReceivedEventHandler>(this, &Page::onMsgReceived);
    webview->add_WebMessageReceived(msgReceivedCB.Get(), nullptr);

    auto reqPermissionCB = Callback<ICoreWebView2PermissionRequestedEventHandler>(this, &Page::onRequestPermission);
    webview->add_PermissionRequested(reqPermissionCB.Get(), nullptr);

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
    // 别叫 args：本函数的形参就叫 args，同名变量在函数体里属于重定义（C2082）
    auto params = Util::msgArgs(param);
    std::wstring methodStr{ method.c_str() };

    // method → handler 注册表分发（命名与清单见 Arch/21-ipc.md）
    try
    {
        const auto& table = msgHandlers();
        auto it = table.find(methodStr);
        if (it != table.end())
        {
            // 返回 true = handler 自己已经发过回包（image.dir 要随包附带目录句柄）
            if (it->second(this, params, result)) return S_OK;
        }
        else
        {
            // 未知方法回一个 error：前端 Msg.invoke 会 reject，而不是静默 resolve(undefined)。
            // 之前"原生侧改了却忘了重新编译 exe"就是被静默吞掉的，补上这条能直接暴露出来
            std::wstring message = L"unknown method: " + methodStr;
            result.SetNamedValue(L"error", JsonValue::CreateStringValue(message));
        }
    }
    catch (const winrt::hresult_error& e)
    {
        // 单个 method 出错不该把进程带走：照样回一个带 id 的 error，前端那边 reject
        std::wstring message = methodStr + L" 处理失败：" + std::wstring(e.message().c_str());
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

