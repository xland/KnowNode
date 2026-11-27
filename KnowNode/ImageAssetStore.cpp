#include "ImageAssetStore.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>
#include <vector>

#include "Env.h"

namespace
{
    namespace fs = std::filesystem;

    fs::path ExtensionFromDataUri(const dui::String& src)
    {
        const size_t end = src.find(L";base64,");
        if (src.compare(0, 11, L"data:image/") != 0 || end == dui::String::npos || end <= 11)
            return L".png";
        const dui::String mime = src.substr(11, end - 11);
        if (mime == L"jpeg" || mime == L"jpg") return L".jpg";
        if (mime == L"gif")  return L".gif";
        if (mime == L"bmp")  return L".bmp";
        if (mime == L"webp") return L".webp";
        if (mime == L"tiff") return L".tif";
        if (mime == L"ico")  return L".ico";
        return L".png";
    }

    fs::path UniquePath(const fs::path& dir, const fs::path& ext)
    {
        static std::atomic<uint64_t> seq{0};
        const auto stamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        for (int i = 0; i < 1000; ++i)
        {
            std::wstring name = L"img_" + std::to_wstring(stamp) + L"_"
                + std::to_wstring(seq.fetch_add(1) + 1) + ext.wstring();
            fs::path path = dir / name;
            std::error_code ec;
            if (!fs::exists(path, ec)) return path;
        }
        return {};
    }

    bool WriteBytes(const fs::path& path, const std::vector<uint8_t>& bytes)
    {
        std::ofstream out(path, std::ios::binary);
        if (!out) return false;
        out.write(reinterpret_cast<const char*>(bytes.data()),
                  static_cast<std::streamsize>(bytes.size()));
        return static_cast<bool>(out);
    }
}

dui::String ImageAssetStore::Save(const dui::String& src)
{
    if (src.empty()) return {};

    std::error_code ec;
    const fs::path dir = Env::getDataPath() / L"imgs";
    fs::create_directories(dir, ec);
    if (ec) return {};

    std::vector<uint8_t> bytes;
    fs::path ext;
    if (dui::richtext::DecodeImageDataUri(src, bytes))
    {
        ext = ExtensionFromDataUri(src);
    }
    else
    {
        const fs::path from(src);
        if (!fs::is_regular_file(from, ec) || ec) return {};
        std::ifstream in(from, std::ios::binary);
        if (!in) return {};
        bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        ext = from.extension();
        if (ext.empty()) ext = L".png";
    }
    if (bytes.empty()) return {};

    const fs::path dest = UniquePath(dir, ext);
    if (dest.empty() || !WriteBytes(dest, bytes)) return {};
    return dui::richtext::MakeFileUri(dest.wstring());
}
