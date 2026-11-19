#pragma once

#include <string>

#include "DUI/DUI.h"

namespace RichDocStore
{
    bool Load(const std::string& utf8Json, dui::richtext::RichDoc& out);
    dui::String Save(const dui::richtext::RichDoc& doc);
}
