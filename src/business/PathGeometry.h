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

} // namespace PathGeometry
