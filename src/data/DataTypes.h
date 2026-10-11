#pragma once

// 数据层类型定义，对应 doc/实施计划.md 的存档 Schema。
// 单位约定：长度 cm（浮点）、时间秒、角度度。
// 角度制式：0° = y 轴正方向（朝下），逆时针增大；数据层只存该制式，
// Qt 角度换算是渲染层的事，本层不做任何换算。

#include <QColor>
#include <QList>
#include <QSizeF>
#include <QString>

#include <optional>

inline constexpr int kFileDataVersion = 2; // v2（阶段10）：新增 prepAreas 节点；v1 存档宽容迁移（缺省 = 空列表）

enum class ObjectType {
    Actor,  // 演员
    Screen, // 屏风
    Prop,   // 道具（仅预留占位，暂不可创建）
    Group,  // 空对象/分组（纯组织节点：不渲染、不可视口选中、不影响子对象坐标）
};

inline QString objectTypeToString(ObjectType type)
{
    switch (type) {
    case ObjectType::Actor:  return QStringLiteral("actor");
    case ObjectType::Screen: return QStringLiteral("screen");
    case ObjectType::Prop:   return QStringLiteral("prop");
    case ObjectType::Group:  return QStringLiteral("group");
    }
    return QStringLiteral("actor");
}

inline bool objectTypeFromString(const QString& str, ObjectType& out)
{
    if (str == QLatin1String("actor"))  { out = ObjectType::Actor;  return true; }
    if (str == QLatin1String("screen")) { out = ObjectType::Screen; return true; }
    if (str == QLatin1String("prop"))   { out = ObjectType::Prop;   return true; }
    if (str == QLatin1String("group"))  { out = ObjectType::Group;  return true; }
    return false;
}

struct Transform {
    double x = 0.0;
    double y = 0.0;
    double rotation = 0.0; // 度，需求角度制
};

inline bool operator==(const Transform& a, const Transform& b)
{
    return a.x == b.x && a.y == b.y && a.rotation == b.rotation;
}
inline bool operator!=(const Transform& a, const Transform& b) { return !(a == b); }

struct Keyframe {
    double t = 0.0;        // 秒
    double x = 0.0;
    double y = 0.0;
    double rotation = 0.0; // 度，需求角度制
};

inline bool operator==(const Keyframe& a, const Keyframe& b)
{
    return a.t == b.t && a.x == b.x && a.y == b.y && a.rotation == b.rotation;
}
inline bool operator!=(const Keyframe& a, const Keyframe& b) { return !(a == b); }

struct ObjectData {
    QString id;
    ObjectType type = ObjectType::Actor;
    QString name;
    QString parentId;                // 父对象 id；空串 = 顶层对象（旧存档缺省即顶层）
    QColor color;                    // JSON 中存 "#rrggbb"
    Transform transform;             // 静态变换：无关键帧时的取值
    std::optional<QSizeF> size;      // 仅屏风有（schema 的 size 节点）
    QList<Keyframe> keyframes;
};

// 层级不变式：FileData.objects 列表顺序 = Hierarchy 的深度优先显示顺序
// （子对象紧跟其父的子树区间），由命令维护；层级只影响 Hierarchy 展示与整理，
// 不影响渲染坐标（子对象坐标绝对，不随父移动，无变换继承）。

inline bool operator==(const ObjectData& a, const ObjectData& b)
{
    return a.id == b.id && a.type == b.type && a.name == b.name && a.parentId == b.parentId
        && a.color == b.color && a.transform == b.transform && a.size == b.size
        && a.keyframes == b.keyframes;
}
inline bool operator!=(const ObjectData& a, const ObjectData& b) { return !(a == b); }

struct StageData {
    double width = 1200.0;        // cm
    double depth = 800.0;         // cm
    double centerOffset = 0.0;    // 中心对称点偏移，cm
    // 舞台矩形范围：x ∈ [centerOffset − width/2, centerOffset + width/2]，y ∈ [0, depth]
};

inline bool operator==(const StageData& a, const StageData& b)
{
    return a.width == b.width && a.depth == b.depth && a.centerOffset == b.centerOffset;
}
inline bool operator!=(const StageData& a, const StageData& b) { return !(a == b); }

// 台下准备区（阶段10）：纯视觉参照的矩形区块，主视口只读展示，只能经设置窗口调整。
// x/y = 矩形中心的场景绝对坐标（与舞台 centerOffset 中心定位的习惯一致）；
// 名称自动命名"准备区N"（不可改名）；颜色从舞台色派生，不单独存储。
struct PrepAreaData {
    QString id;              // prep-NNN（业务层生成，全局唯一）
    QString name;            // 准备区N（自动命名，全局唯一）
    double x = 0.0;          // cm，矩形中心
    double y = 0.0;          // cm，矩形中心
    double width = 400.0;    // cm
    double height = 300.0;   // cm
};

inline bool operator==(const PrepAreaData& a, const PrepAreaData& b)
{
    return a.id == b.id && a.name == b.name && a.x == b.x && a.y == b.y
        && a.width == b.width && a.height == b.height;
}
inline bool operator!=(const PrepAreaData& a, const PrepAreaData& b) { return !(a == b); }

struct SceneLabel {               // 场次标签（纯视觉标记）
    double t = 0.0;               // 秒
    QString name;
};

inline bool operator==(const SceneLabel& a, const SceneLabel& b)
{
    return a.t == b.t && a.name == b.name;
}
inline bool operator!=(const SceneLabel& a, const SceneLabel& b) { return !(a == b); }

struct TimelineData {
    double duration = 60.0;       // 秒
    QList<SceneLabel> labels;
};

inline bool operator==(const TimelineData& a, const TimelineData& b)
{
    return a.duration == b.duration && a.labels == b.labels;
}
inline bool operator!=(const TimelineData& a, const TimelineData& b) { return !(a == b); }

struct FileData {
    int version = kFileDataVersion;
    StageData stage;
    QList<ObjectData> objects;
    QList<PrepAreaData> prepAreas; // 台下准备区（v2 新增；v1 存档迁移为空列表）
    TimelineData timeline;

    // 新建文件默认值：舞台 1200×800cm、centerOffset=0、时间轴 60s、空对象、空准备区、空场次标签
    static FileData createDefault() { return FileData{}; }
};

inline bool operator==(const FileData& a, const FileData& b)
{
    return a.version == b.version && a.stage == b.stage && a.objects == b.objects
        && a.prepAreas == b.prepAreas // dirty 内容比较依赖本字段，漏加 = dirty 跟踪静默失效
        && a.timeline == b.timeline;
}
inline bool operator!=(const FileData& a, const FileData& b) { return !(a == b); }
