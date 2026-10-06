#pragma once
#include "../SQLite/sqlite3.h"
#include <cstdint>
#include <string>

/**
 * setting 表：键值形式的设置项，存放需要持久化的 UI 状态等信息
 * （如 KnowDetail 面板是否钉住、各面板宽度）。新增设置项直接加 key，不必改表结构。
 */
class Setting
{
public:
    static Setting& instance();

    /// 建表（由 Db::init 调用）
    void create(sqlite3* conn);

    /// 读设置项；不存在时返回 fallback
    std::string get(const std::string& key, const std::string& fallback = std::string()) const;
    /// 写设置项（不存在则插入，存在则更新）
    void set(const std::string& key, const std::string& value);

    /// 便捷读取：读整数设置项
    int64_t getInt(const std::string& key, int64_t fallback = 0) const;

private:
    Setting();
};
