#pragma once

// 移动路径几何（阶段12，业务层纯函数，不依赖 Widgets）：复合曲线（直线/圆弧段）
// 采样折线化 + 累计弧长表 + 弧长比例定位/投影。渲染（StageScene 白线/路径点）
// 与插值（TimelinePlayer 弧长匀速）共用同一份采样实现，保证数据源一致。
//
// 弧线语义：每段（起点→P1→…→终点）独立一条圆弧，由该段圆心角 θ（度，有符号，
// 正 = 沿行进方向左侧鼓出）确定；弧半径 = 弦长 / (2·sin(θ/2))。
// |θ| < 0.1° 按直线处理（避免半径爆炸）；θ 钳制 ±350°。

#include "TimelinePlayer.h" // kTimeEpsilon
#include "data/DataTypes.h"

#include <QList>
#include <QPainterPath>
#include <QPointF>
#include <QString>

#include <algorithm>
#include <cmath>
#include <limits>

namespace PathGeometry {

constexpr double kMinLegAngleDeg = 0.1;   // |θ| 小于此按直线
constexpr double kMaxLegAngleDeg = 350.0; // 圆心角钳制上限（±180 = 半圆）
constexpr double kDefaultLegAngleDeg = 30.0; // 直线切弧线时各段的初始圆心角
constexpr int kSamplesPerLeg = 32;        // 每段折线化采样数（≥32，弧长精度）

constexpr double kPi = 3.14159265358979323846;

inline double clampLegAngle(double angleDeg)
{
    return qBound(-kMaxLegAngleDeg, angleDeg, kMaxLegAngleDeg);
}

inline bool legIsArc(double angleDeg)
{
    return std::abs(angleDeg) >= kMinLegAngleDeg;
}

// 段查找：对象 id + 起止时刻（kTimeEpsilon 判定）
inline const PathData* findPathData(const QList<PathData>& paths, const QString& objectId,
                                    double from, double to)
{
    for (const PathData& path : paths) {
        if (path.objectId == objectId
            && qAbs(path.from - from) <= TimelinePlayer::kTimeEpsilon
            && qAbs(path.to - to) <= TimelinePlayer::kTimeEpsilon)
            return &path;
    }
    return nullptr;
}

inline PathData* findPathData(QList<PathData>& paths, const QString& objectId,
                              double from, double to)
{
    for (PathData& path : paths) {
        if (path.objectId == objectId
            && qAbs(path.from - from) <= TimelinePlayer::kTimeEpsilon
            && qAbs(path.to - to) <= TimelinePlayer::kTimeEpsilon)
            return &path;
    }
    return nullptr;
}

// 单段采样：A→B 圆心角 θ，输出含端点的 kSamplesPerLeg+1 个折线点；
// 直线段（|θ|<0.1° 或 A=B）退化为两端点
inline QList<QPointF> sampleLeg(const QPointF& a, const QPointF& b, double angleDeg)
{
    if (!legIsArc(angleDeg) || a == b)
        return { a, b };
    const QPointF chord = b - a;
    const double c = std::hypot(chord.x(), chord.y());
    const double theta = angleDeg * kPi / 180.0;
    const double radius = c / (2.0 * std::sin(theta / 2.0)); // 有符号（θ<0 → 半径负）
    const QPointF mid = (a + b) / 2.0;
    // 行进方向左侧法线（坐标系 y 向下）：dir=(1,0) 时左侧 = (0,-1)
    const QPointF normal(chord.y() / c, -chord.x() / c);
    // 圆心：劣弧在鼓出侧对面，优弧（|θ|>180°，cos(θ/2)<0）在同侧（符号自然处理）
    const QPointF center = mid - normal * (radius * std::cos(theta / 2.0));
    const double r = std::abs(radius);
    const double startAngle = std::atan2(a.y() - center.y(), a.x() - center.x());
    QList<QPointF> points;
    points.reserve(kSamplesPerLeg + 1);
    for (int i = 0; i <= kSamplesPerLeg; ++i) {
        const double phi = startAngle + theta * double(i) / double(kSamplesPerLeg);
        points.append(center + QPointF(std::cos(phi), std::sin(phi)) * r);
    }
    return points;
}

// 复合曲线折线化：端点（两帧数据值）+ 路径点 + 每段圆心角 → 折线点列
// （含首尾；段间连接点不重复）。type=Line 时忽略角度（折线路径）。
inline QList<QPointF> samplePath(const PathData& path, const QPointF& from,
                                 const QPointF& to)
{
    QList<QPointF> result;
    QPointF legStart = from;
    for (int i = 0; i <= path.points.size(); ++i) {
        const QPointF legEnd = i < path.points.size()
            ? QPointF(path.points.at(i).x, path.points.at(i).y)
            : to;
        const double angle = (path.type == PathType::Arc && i < path.legAngles.size())
            ? path.legAngles.at(i)
            : 0.0;
        QList<QPointF> leg = sampleLeg(legStart, legEnd, angle);
        if (!result.isEmpty() && !leg.isEmpty())
            leg.removeFirst(); // 段间连接点不重复
        result += leg;
        legStart = legEnd;
    }
    return result;
}

// 累计弧长表（与折线点列等长，首元素 0；空输入 → 空表）
inline QList<double> cumulativeLengths(const QList<QPointF>& polyline)
{
    QList<double> cum;
    if (polyline.isEmpty())
        return cum;
    cum.reserve(polyline.size());
    cum.append(0.0);
    double total = 0.0;
    for (int i = 1; i < polyline.size(); ++i) {
        total += std::hypot(polyline.at(i).x() - polyline.at(i - 1).x(),
                            polyline.at(i).y() - polyline.at(i - 1).y());
        cum.append(total);
    }
    return cum;
}

// 弧长比例 → 位置（累计弧长表二分定位 + 段内线性插值；ratio 钳制 [0,1]）
inline QPointF pointAtRatio(const QList<QPointF>& polyline, const QList<double>& cum,
                            double ratio)
{
    if (polyline.isEmpty())
        return QPointF();
    if (polyline.size() == 1 || cum.isEmpty())
        return polyline.first();
    const double total = cum.last();
    if (total <= 0.0)
        return polyline.first();
    const double target = qBound(0.0, ratio, 1.0) * total;
    const auto it = std::lower_bound(cum.begin(), cum.end(), target);
    int index = int(it - cum.begin());
    if (index <= 0)
        return polyline.first();
    if (index >= polyline.size())
        return polyline.last();
    const double segLen = cum.at(index) - cum.at(index - 1);
    const double f = segLen > 0.0 ? (target - cum.at(index - 1)) / segLen : 0.0;
    return polyline.at(index - 1) + (polyline.at(index) - polyline.at(index - 1)) * f;
}

// 点到折线的投影 → 弧长比例（最近点所在段 + 段内比例；拖拽回显 t 用）
inline double projectToRatio(const QList<QPointF>& polyline, const QPointF& p)
{
    if (polyline.size() < 2)
        return 0.0;
    const QList<double> cum = cumulativeLengths(polyline);
    const double total = cum.last();
    if (total <= 0.0)
        return 0.0;
    double bestDist = std::numeric_limits<double>::max();
    double bestRatio = 0.0;
    for (int i = 0; i + 1 < polyline.size(); ++i) {
        const QPointF a = polyline.at(i);
        const QPointF ab = polyline.at(i + 1) - a;
        const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
        double f = 0.0;
        if (len2 > 0.0) {
            f = qBound(0.0,
                       ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / len2,
                       1.0);
        }
        const QPointF proj = a + ab * f;
        const double dist = std::hypot(p.x() - proj.x(), p.y() - proj.y());
        if (dist < bestDist) {
            bestDist = dist;
            bestRatio = (cum.at(i) + std::sqrt(len2) * f) / total;
        }
    }
    return bestRatio;
}

// 折线总长（cm）
inline double totalLength(const QList<QPointF>& polyline)
{
    const QList<double> cum = cumulativeLengths(polyline);
    return cum.isEmpty() ? 0.0 : cum.last();
}

// ---- 道具形状合围校验（阶段13 任务2）----
// 编辑器实时校验与存档解析共用判定来源（数据层解析只做简单校验的同一子集）。
// 规则：顶点 ≥3（或恰 2 顶点且两边均为弧 = 圆形模板）；无零长边（相邻顶点间距
// > 1cm）；非相邻边两两不相交（弧边先采样折线化——弧边鼓出导致的自交必须被
// 采样判定捕获）；|有向面积| > 1cm²（防共线退化）。返回首处非法原因供红字提示。
struct ShapeValidity {
    bool valid = true;
    QString message; // 非法原因（合法时空串）
};

// 线段相交判定（含端点接触/共线重叠：非相邻边不共享端点，接触即非法）
inline bool segmentsIntersect(const QPointF& p1, const QPointF& p2,
                              const QPointF& p3, const QPointF& p4)
{
    const auto cross = [](const QPointF& o, const QPointF& a, const QPointF& b) {
        return (a.x() - o.x()) * (b.y() - o.y()) - (a.y() - o.y()) * (b.x() - o.x());
    };
    const auto onSegment = [](const QPointF& a, const QPointF& b, const QPointF& p) {
        return qMin(a.x(), b.x()) - 1e-9 <= p.x() && p.x() <= qMax(a.x(), b.x()) + 1e-9
            && qMin(a.y(), b.y()) - 1e-9 <= p.y() && p.y() <= qMax(a.y(), b.y()) + 1e-9;
    };
    const double d1 = cross(p3, p4, p1);
    const double d2 = cross(p3, p4, p2);
    const double d3 = cross(p1, p2, p3);
    const double d4 = cross(p1, p2, p4);
    if (((d1 > 0) != (d2 > 0)) && ((d3 > 0) != (d4 > 0)))
        return true; // 严格相交
    // 端点接触 / 共线重叠
    if (qAbs(d1) < 1e-9 && onSegment(p3, p4, p1)) return true;
    if (qAbs(d2) < 1e-9 && onSegment(p3, p4, p2)) return true;
    if (qAbs(d3) < 1e-9 && onSegment(p1, p2, p3)) return true;
    if (qAbs(d4) < 1e-9 && onSegment(p1, p2, p4)) return true;
    return false;
}

// 有向面积（采样折线上鞋带公式；本坐标系 y 向下，视觉逆时针为负值）
inline double signedArea(const QList<QPointF>& polyline)
{
    double sum = 0.0;
    for (int i = 0; i < polyline.size(); ++i) {
        const QPointF& a = polyline.at(i);
        const QPointF& b = polyline.at((i + 1) % polyline.size());
        sum += a.x() * b.y() - b.x() * a.y();
    }
    return sum / 2.0;
}

inline ShapeValidity validatePropShape(const PropShape& shape)
{
    const int count = shape.vertices.size();
    if (count < 2)
        return { false, QStringLiteral("顶点数不足（至少 2 个）") };
    if (count == 2) {
        // 圆形模板：2 顶点 + 两条弧边；直边则退化（往返重合）
        const bool bothArc = legIsArc(shape.edgeAngles.value(0))
            && legIsArc(shape.edgeAngles.value(1));
        if (!bothArc)
            return { false, QStringLiteral("2 顶点的形状两条边都必须是弧线") };
    }
    // 零长边
    for (int i = 0; i < count; ++i) {
        const PathPoint& a = shape.vertices.at(i);
        const PathPoint& b = shape.vertices.at((i + 1) % count);
        if (std::hypot(b.x - a.x, b.y - a.y) <= 1.0)
            return { false, QStringLiteral("边 %1 长度为零（≤1cm）").arg(i + 1) };
    }
    // 逐边采样折线化（弧边鼓出的自交必须被采样捕获）
    QList<QList<QPointF>> edgePolys;
    edgePolys.reserve(count);
    for (int i = 0; i < count; ++i) {
        const PathPoint& a = shape.vertices.at(i);
        const PathPoint& b = shape.vertices.at((i + 1) % count);
        const double angle = i < shape.edgeAngles.size() ? shape.edgeAngles.at(i) : 0.0;
        edgePolys.append(sampleLeg(QPointF(a.x, a.y), QPointF(b.x, b.y), angle));
    }
    // 非相邻边两两不相交（相邻 = 共享顶点的边，含首尾环绕）
    for (int i = 0; i < count; ++i) {
        for (int j = i + 2; j < count; ++j) {
            if (i == 0 && j == count - 1)
                continue; // 首尾相邻（环绕共享顶点）
            for (int s = 0; s + 1 < edgePolys.at(i).size(); ++s) {
                for (int t = 0; t + 1 < edgePolys.at(j).size(); ++t) {
                    if (segmentsIntersect(edgePolys.at(i).at(s), edgePolys.at(i).at(s + 1),
                                          edgePolys.at(j).at(t), edgePolys.at(j).at(t + 1))) {
                        return { false, QStringLiteral("边 %1 与边 %2 相交").arg(i + 1).arg(j + 1) };
                    }
                }
            }
        }
    }
    // 退化：|有向面积| 阈值（采样折线，含弧边面积；防共线退化）
    QList<QPointF> outline;
    for (const QList<QPointF>& leg : edgePolys) {
        for (int i = (outline.isEmpty() ? 0 : 1); i < leg.size(); ++i)
            outline.append(leg.at(i));
    }
    if (std::abs(signedArea(outline)) <= 1.0)
        return { false, QStringLiteral("形状退化（面积过小或顶点共线）") };
    return {};
}

// 顶点绕向归一为逆时针（写入存档/渲染前调用）：本坐标系 y 向下，视觉逆时针
// 对应鞋带负有向面积；反转顶点序并重排边角度（边反向行进 → 左侧鼓出符号翻转）。
// 2 顶点形状（圆形模板）绕向无意义，不动
inline void normalizePropShapeWinding(PropShape& shape)
{
    const int count = shape.vertices.size();
    if (count < 3)
        return;
    while (shape.edgeAngles.size() < count)
        shape.edgeAngles.append(0.0); // 防御：长度不变式
    QList<QPointF> points;
    for (const PathPoint& v : shape.vertices)
        points.append(QPointF(v.x, v.y));
    if (signedArea(points) <= 0.0)
        return; // 已是逆时针（y 向下为负）
    QList<PathPoint> reversedVertices;
    QList<double> reversedAngles;
    for (int i = 0; i < count; ++i) {
        reversedVertices.append(shape.vertices.at(count - 1 - i));
        // 新边 i = 旧边 (count-2-i) 反向（旧边 v[count-2-i]→v[count-1-i]）
        reversedAngles.append(-shape.edgeAngles.at((count - 2 - i + count) % count));
    }
    shape.vertices = reversedVertices;
    shape.edgeAngles = reversedAngles;
}

// ---- 站立阻挡判定（阶段13 任务3，纯函数）----
// 障碍物 = 全部屏风（旋转矩形）∪ standable=false 道具（顶点×scale 后的实际
// 多边形，弧边采样折线化）；演员互不阻挡。判定 = 段内等分 10 时刻扫掠：
// 每时刻障碍物按 TimelinePlayer 插值取位姿，路径采样折线（与渲染/插值同一实现）
// 的点（含两端点）落入障碍物多边形 或 折线与障碍物边相交 → 阻挡。
// 不溯及既往：只约束路径编辑操作（由 PathCommands 提交前调用），已有路径不动。
struct ObstaclePolygon {
    QString objectId;
    QList<QPointF> localOutline; // 局部多边形顶点（未旋转/平移，原点 = 对象位置）
};

// 局部点 → 场景：需求角度制旋转 + 平移。朝向向量 = (−sinθ, cosθ)（0°=+y 朝下，
// 逆时针增大），局部 +y = 前、+x = 右 = (−cosθ, −sinθ)（与 RenderUtil 同推导，
// 业务层自含实现，不依赖渲染层）
inline QPointF transformPoint(const QPointF& local, double x, double y, double rotationDeg)
{
    const double rad = rotationDeg * kPi / 180.0;
    const double s = std::sin(rad);
    const double c = std::cos(rad);
    return QPointF(x + local.x() * (-c) + local.y() * (-s),
                   y + local.x() * (-s) + local.y() * c);
}

// 障碍物集合（排除 excludeId——被编辑路径的对象自身不是自己的障碍物）
inline QList<ObstaclePolygon> collectObstacles(const FileData& data,
                                               const QString& excludeId)
{
    QList<ObstaclePolygon> result;
    for (const ObjectData& object : data.objects) {
        if (object.id == excludeId)
            continue;
        if (object.type == ObjectType::Screen) {
            // 屏风永远阻挡：旋转矩形 4 顶点（无开关）
            const double w = object.size ? object.size->width() : 100.0;
            const double h = object.size ? object.size->height() : 10.0;
            result.append(
                { object.id,
                  { { -w / 2.0, -h / 2.0 }, { -w / 2.0, h / 2.0 },
                    { w / 2.0, h / 2.0 }, { w / 2.0, -h / 2.0 } } });
        } else if (object.type == ObjectType::Prop && !object.standable) {
            // 不可站立道具阻挡：实际多边形（顶点×scale，弧边采样折线化）
            const PropShape shape = object.shape.value_or(defaultPropShape());
            QList<QPointF> outline;
            const int count = shape.vertices.size();
            for (int i = 0; i < count; ++i) {
                const PathPoint& a = shape.vertices.at(i);
                const PathPoint& b = shape.vertices.at((i + 1) % count);
                const QPointF pa(a.x * object.scaleX, a.y * object.scaleY);
                const QPointF pb(b.x * object.scaleX, b.y * object.scaleY);
                const double angle =
                    i < shape.edgeAngles.size() ? shape.edgeAngles.at(i) : 0.0;
                const QList<QPointF> leg = sampleLeg(pa, pb, angle);
                for (int j = (outline.isEmpty() ? 0 : 1); j < leg.size(); ++j)
                    outline.append(leg.at(j));
            }
            result.append({ object.id, outline });
        }
    }
    return result;
}

// 段路径扫掠阻挡判定（纯函数）：polyline = 复合曲线采样折线（PathGeometry
// 同一实现）；段内等分 10 时刻（含两端点时刻），障碍物该时刻插值位姿下的
// 多边形与路径点/折线相交 → true
inline bool pathBlocked(const FileData& data, const QString& objectId, double from,
                        double to, const QList<QPointF>& polyline)
{
    if (polyline.size() < 2)
        return false;
    const QList<ObstaclePolygon> obstacles = collectObstacles(data, objectId);
    if (obstacles.isEmpty())
        return false;
    for (int k = 0; k <= 10; ++k) { // 段内等分 10 时刻扫掠（含两端点）
        const double t = from + (to - from) * k / 10.0;
        for (const ObstaclePolygon& obstacle : obstacles) {
            const ObjectData* object = nullptr;
            for (const ObjectData& entry : data.objects) {
                if (entry.id == obstacle.objectId) {
                    object = &entry;
                    break;
                }
            }
            if (!object)
                continue;
            const Transform pose =
                TimelinePlayer::interpolate(*object, data.paths, t);
            // 障碍物该时刻位姿下的场景多边形
            QList<QPointF> scenePoly;
            for (const QPointF& p : obstacle.localOutline)
                scenePoly.append(transformPoint(p, pose.x, pose.y, pose.rotation));
            // 路径点（含两端点）落入障碍物
            QPainterPath obstaclePath;
            obstaclePath.moveTo(scenePoly.first());
            for (int i = 1; i < scenePoly.size(); ++i)
                obstaclePath.lineTo(scenePoly.at(i));
            obstaclePath.closeSubpath();
            for (const QPointF& p : polyline) {
                if (obstaclePath.contains(p))
                    return true;
            }
            // 路径折线与障碍物边相交
            for (int i = 0; i + 1 < polyline.size(); ++i) {
                for (int j = 0; j < scenePoly.size(); ++j) {
                    if (segmentsIntersect(polyline.at(i), polyline.at(i + 1),
                                          scenePoly.at(j),
                                          scenePoly.at((j + 1) % scenePoly.size())))
                        return true;
                }
            }
        }
    }
    return false;
}

} // namespace PathGeometry
