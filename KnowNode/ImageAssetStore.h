#pragma once

#include "DUI/DUI.h"

namespace ImageAssetStore
{
    // 把图片 src（本地路径或 data URI）保存到数据目录 imgs，
    // 返回可用于富文本文档的 file:// URI；失败返回空串。
    dui::String Save(const dui::String& src);
}
