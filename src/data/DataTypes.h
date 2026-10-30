#pragma once

// 数据层类型定义，对应 doc/实施方案.md 的存档 Schema。
// 单位约定：长度 cm（浮点）、时间秒、角度度。
// 角度制式：0° = y 轴正方向（朝下），逆时针增大；数据层只存该制式，
// Qt 角度换算是渲染层的事，本层不做任何换算。

#include <QColor>
#include <QList>
#include <QSizeF>
#include <QString>

#include <optional>

inline constexpr int kFileDataVersion = 6; // v6（阶段14）：FileData 新增资源库（resources）；v1~v5 缺 resources 节点宽容迁移为空列表

enum class ObjectType {
    Actor,  // 演员
    Screen, // 屏风
    Prop,   // 舞台道具
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

// 路径点（阶段12）：cm 场景绝对坐标；阶段13 起也用作道具形状的局部顶点
struct PathPoint {
    double x = 0.0;
    double y = 0.0;
};

inline bool operator==(const PathPoint& a, const PathPoint& b)
{
    return a.x == b.x && a.y == b.y;
}
inline bool operator!=(const PathPoint& a, const PathPoint& b) { return !(a == b); }

// ---- 舞台道具形状（阶段13，v5 新增；仅 ObjectType::Prop 有意义）----
// 多边形 + 弧边：局部坐标（以对象位置为原点、未缩放），顶点统一逆时针；
// 边 i = v[i] → v[(i+1)%n]，边的行进方向即顶点绕向；edgeAngles[i] 为边 i 的
// 圆心角（度，符号约定同路径编辑：正 = 沿边行进方向左侧鼓出），长度恒 =
// vertices.size()（圆 = 2 顶点 + 两条 180° 弧边）。
struct PropShape {
    QList<PathPoint> vertices;
    QList<double> edgeAngles;
};

inline bool operator==(const PropShape& a, const PropShape& b)
{
    return a.vertices == b.vertices && a.edgeAngles == b.edgeAngles;
}
inline bool operator!=(const PropShape& a, const PropShape& b) { return !(a == b); }

// 默认道具形状：100×100cm 矩形（局部 ±50，顶点绕向逆时针 = 行进方向左侧为内部），
// 直边（圆心角全 0）
inline PropShape defaultPropShape()
{
    return { { { -50.0, -50.0 }, { -50.0, 50.0 }, { 50.0, 50.0 }, { 50.0, -50.0 } },
             { 0.0, 0.0, 0.0, 0.0 } };
}

struct ObjectData {
    QString id;
    ObjectType type = ObjectType::Actor;
    QString name;
    QString parentId;                // 父对象 id；空串 = 顶层对象（旧存档缺省即顶层）
    QColor color;                    // JSON 中存 "#rrggbb"
    Transform transform;             // 静态变换：无关键帧时的取值
    std::optional<QSizeF> size;      // 仅屏风有（schema 的 size 节点）
    QList<Keyframe> keyframes;
    // ---- 道具字段（阶段13，v5；仅 type=Prop 有意义，其余类型恒缺省）----
    std::optional<PropShape> shape;  // 多边形+弧边形状；缺省 = 默认 100×100cm 矩形
    double scaleX = 1.0;             // 宽度缩放倍数（0.1~100，1 = 无缩放）
    double scaleY = 1.0;             // 深度缩放倍数
    bool standable = true;           // 是否可供站立；false 时在路径编辑与位置变更提交中作为障碍物
};

// 层级不变式：FileData.objects 列表顺序 = Hierarchy 的深度优先显示顺序
// （子对象紧跟其父的子树区间），由命令维护；层级只影响 Hierarchy 展示与整理，
// 不影响渲染坐标（子对象坐标绝对，不随父移动，无变换继承）。

inline bool operator==(const ObjectData& a, const ObjectData& b)
{
    return a.id == b.id && a.type == b.type && a.name == b.name && a.parentId == b.parentId
        && a.color == b.color && a.transform == b.transform && a.size == b.size
        && a.keyframes == b.keyframes
        && a.shape == b.shape && a.scaleX == b.scaleX && a.scaleY == b.scaleY
        && a.standable == b.standable; // 道具字段（v5）：漏加 = dirty 跟踪静默失效
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
// 名称自动命名"准备区N"（不可改名）。
// color（阶段11，v3 新增）：每区独立填充色；invalid = 派生态（跟随舞台色，
// 渲染时回退 stageColor().darker(160)）。数据层不知道 AppSettings（零交叉约定），
// v1/v2 迁移与新建均为派生态，不烘入具体色值。
struct PrepAreaData {
    QString id;              // prep-NNN（业务层生成，全局唯一）
    QString name;            // 准备区N（自动命名，全局唯一）
    double x = 0.0;          // cm，矩形中心
    double y = 0.0;          // cm，矩形中心
    double width = 400.0;    // cm
    double height = 300.0;   // cm
    QColor color;            // invalid = 派生态（跟随舞台色）；JSON 中存 "#rrggbb" 或空串
};

inline bool operator==(const PrepAreaData& a, const PrepAreaData& b)
{
    return a.id == b.id && a.name == b.name && a.x == b.x && a.y == b.y
        && a.width == b.width && a.height == b.height
        && a.color == b.color; // dirty 判定依赖：漏加 = 颜色改动不弄脏文件
}
inline bool operator!=(const PrepAreaData& a, const PrepAreaData& b) { return !(a == b); }

// ---- 移动路径（阶段12，v4 新增）----
// 路径按"段"挂接：对象 id + 相邻两帧的起止时刻（端点不存，派生自关键帧数据值）。
// 零存储默认态：type=Line 且 points 为空的段不存在 PathData（内存亦不创建）——
// 无 PathData 的段插值走原直线匀速路径，与 v3 及更早存档观感逐位一致。
// 名称派生（<对象名>_<起>s至<止>s）不存储；t 参数（弧长百分比）派生不存档。
enum class PathType {
    Line, // 直线（途经路径点的折线）
    Arc,  // 弧线（每段独立圆弧，圆心角见 legAngles）
};

inline QString pathTypeToString(PathType type)
{
    return type == PathType::Arc ? QStringLiteral("arc") : QStringLiteral("line");
}

inline PathType pathTypeFromString(const QString& str)
{
    return str == QLatin1String("arc") ? PathType::Arc : PathType::Line; // 非法宽容 Line
}

struct PathData {
    QString objectId;
    double from = 0.0;         // 段起始帧时刻（秒，kTimeEpsilon 判定）
    double to = 0.0;           // 段终止帧时刻
    PathType type = PathType::Line;
    QList<PathPoint> points;   // 路径点，沿行进方向有序
    QList<double> legAngles;   // 每段圆心角（度，有符号，正 = 沿行进方向左侧鼓出）；
                               // 长度恒 = points.size()+1（仅 Arc 有意义；±350 钳制、
                               // |θ|<0.1° 按直线由几何层处理）
};

inline bool operator==(const PathData& a, const PathData& b)
{
    return a.objectId == b.objectId && a.from == b.from && a.to == b.to
        && a.type == b.type && a.points == b.points && a.legAngles == b.legAngles;
        // dirty 判定依赖：漏字段 = dirty 跟踪静默失效
}
inline bool operator!=(const PathData& a, const PathData& b) { return !(a == b); }

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

// ---- 项目资源库（阶段14，v6 新增）----
// 支持音频和图片；资源清单随存档保存，实际文件复制到项目根目录的
// Assets/audio 或 Assets/images，JSON 只存相对路径。id 为全库唯一 UUID；
// name 为 trim 后非空且项目内唯一的别名，originalName 仅保存原始文件名
// （不保存外部源路径）。文件大小/尺寸/时长/解码与播放状态均为运行时数据，
// 不进入 JSON。
enum class ResourceType {
    Audio,
    Image,
};

inline QString resourceTypeToString(ResourceType type)
{
    return type == ResourceType::Image ? QStringLiteral("image") : QStringLiteral("audio");
}

inline bool resourceTypeFromString(const QString& str, ResourceType& out)
{
    if (str == QLatin1String("audio")) { out = ResourceType::Audio; return true; }
    if (str == QLatin1String("image")) { out = ResourceType::Image; return true; }
    return false;
}

struct ResourceData {
    QString id;
    ResourceType type = ResourceType::Audio;
    QString name;
    QString originalName;
    QString relativePath; // 资源根目录内的相对路径（"/" 分隔），禁止绝对路径与 ..
};

inline bool operator==(const ResourceData& a, const ResourceData& b)
{
    return a.id == b.id && a.type == b.type && a.name == b.name
        && a.originalName == b.originalName && a.relativePath == b.relativePath;
}
inline bool operator!=(const ResourceData& a, const ResourceData& b) { return !(a == b); }

struct FileData {
    int version = kFileDataVersion;
    StageData stage;
    QList<ObjectData> objects;
    QList<PrepAreaData> prepAreas; // 台下准备区（v2 新增；v1 存档迁移为空列表）
    QList<PathData> paths;         // 移动路径（v4 新增；v3 及更早存档迁移为空列表）
    QList<ResourceData> resources; // 项目资源库（v6 新增；v1~v5 迁移为空列表）
    TimelineData timeline;

    // 新建文件默认值：舞台 1200×800cm、centerOffset=0、时间轴 60s、空对象、空准备区、
    // 空路径、空资源库、空场次标签
    static FileData createDefault() { return FileData{}; }
};

inline bool operator==(const FileData& a, const FileData& b)
{
    return a.version == b.version && a.stage == b.stage && a.objects == b.objects
        && a.prepAreas == b.prepAreas // dirty 内容比较依赖本字段，漏加 = dirty 跟踪静默失效
        && a.paths == b.paths        // 同上（v4）
        && a.resources == b.resources // 同上（v6：漏加 = 资源修改不弄脏存档）
        && a.timeline == b.timeline;
}
inline bool operator!=(const FileData& a, const FileData& b) { return !(a == b); }
