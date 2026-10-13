#pragma once

// 存档读写（QJsonDocument）。任何损坏输入只返回错误枚举，不崩溃。
// 根节点 version 字段在加载时检查：受理区间 [1, kFileDataVersion]（当前 3），
// 低版本存档宽容迁移（v1 无 prepAreas 节点 → 空列表；v2 prepAreas 无 color 键
// → 派生态 invalid），高版本拒载。

#include "DataTypes.h"

#include <QJsonObject>
#include <QString>

enum class LoadError {
    None,               // 成功
    Io,                 // 文件打不开 / 读写失败
    Parse,              // JSON 语法错误 / 根节点不是对象
    UnsupportedVersion, // version 缺失或不受支持
    InvalidData,        // JSON 合法但结构/字段类型不符 schema
};

struct LoadResult {
    LoadError error = LoadError::None;
    QString message;
    FileData data;

    bool ok() const { return error == LoadError::None; }
};

struct SaveResult {
    bool ok = true;
    QString message;
};

class JsonStore {
public:
    static constexpr int currentVersion = kFileDataVersion;

    static LoadResult load(const QString& path);
    static SaveResult save(const QString& path, const FileData& data);

    // 公开以便单测与后续复用；fromJson 内含 version 检查与结构校验
    static QJsonObject toJson(const FileData& data);
    static LoadResult fromJson(const QJsonObject& root);
};
