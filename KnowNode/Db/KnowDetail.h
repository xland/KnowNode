#pragma once
#include "../SQLite/sqlite3.h"
#include <cstdint>
#include <string>

/**
 * know_detail 表：富文本详情。
 * 节点（标题之外的正文）与连线（对这条关联的描述）各持有一条。
 * 详情单独成表，是为了把这类大字段从节点/连线里分出去——查列表时不必带上正文。
 */
class KnowDetail
{
public:
    static KnowDetail& instance();

    /// 建表（由 Db::init 调用）
    void create(sqlite3* conn);

    /// 新建一条详情，返回新 id；失败返回 0
    int64_t add(const std::string& content = std::string());
    /// 覆盖保存正文
    bool save(int64_t id, const std::string& content);
    /// 读正文；不存在返回空串
    std::string content(int64_t id) const;
    /// 删除详情；调用方需保证已没有节点/连线指向它（外键默认是 RESTRICT）
    bool remove(int64_t id);

private:
    KnowDetail();
};
