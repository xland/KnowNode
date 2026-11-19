#include <system_error>

#include <shlobj.h>
#include <windows.h>

#include "Db/Db.h"
#include "Env.h"

std::filesystem::path Env::dataPath;

void Env::init()
{
    initDataPath();
    Db::instance().init();
}

std::filesystem::path Env::getDataPath()
{
    return dataPath;
}

void Env::initDataPath()
{
    PWSTR path = nullptr;
    const HRESULT hr = SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &path);
    if (FAILED(hr) || path == nullptr)
    {
        MessageBoxW(nullptr, L"无法获取应用数据目录", L"KnowNode", MB_OK | MB_ICONERROR);
        ExitProcess(1);
    }

    dataPath = path;
    CoTaskMemFree(path);
    dataPath /= L"KnowNode";

    std::error_code ec;
    std::filesystem::create_directories(dataPath, ec);
    if (ec)
    {
        const std::wstring msg = L"无法创建应用数据目录：\n" + dataPath.wstring();
        MessageBoxW(nullptr, msg.c_str(), L"KnowNode", MB_OK | MB_ICONERROR);
        ExitProcess(1);
    }
}
