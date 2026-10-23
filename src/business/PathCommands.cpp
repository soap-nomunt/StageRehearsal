#include "PathCommands.h"

namespace {

PathData* findPath(FileData& data, const QString& objectId, double from, double to)
{
    return PathGeometry::findPathData(data.paths, objectId, from, to);
}

// 段有效 PathData 下标（无则 -1）
int pathIndexAt(const FileData& data, const QString& objectId, double from, double to)
{
    for (int i = 0; i < data.paths.size(); ++i) {
        const PathData& path = data.paths.at(i);
        if (path.objectId == objectId
            && qAbs(path.from - from) <= TimelinePlayer::kTimeEpsilon
            && qAbs(path.to - to) <= TimelinePlayer::kTimeEpsilon)
            return i;
    }
    return -1;
}

// 当前复合曲线折线（无 PathData = 默认直线；供加点/改 t 的投影与滑动）
QList<QPointF> currentPolyline(const FileData& data, const QString& objectId, double from,
                               double to, const QPointF& fromPos, const QPointF& toPos)
{
    if (const PathData* path = PathGeometry::findPathData(data.paths, objectId, from, to))
        return PathGeometry::samplePath(*path, fromPos, toPos);
    return { fromPos, toPos };
}

// 阶段13 任务3 阻挡校验（提交前调用，true = 拒绝编辑）：编辑后的段路径与障碍物
// 扫掠相交判定（PathGeometry::pathBlocked 同一采样实现）；newPath = nullptr 表示
// 编辑后回退为直线（无 PathData）。段帧缺失不在此校验（原有逻辑各自兜底）。
// 不溯及既往：只约束本次编辑，已有路径不动。
bool segmentBlockedAfter(const FileData& data, const QString& objectId, double from,
                         double to, const PathData* newPath)
{
    QPointF fromPos;
    QPointF toPos;
    if (!resolveSegmentEndpoints(data, objectId, from, to, fromPos, toPos))
        return false;
    const QList<QPointF> polyline =
        newPath ? PathGeometry::samplePath(*newPath, fromPos, toPos)
                : QList<QPointF> { fromPos, toPos };
    return PathGeometry::pathBlocked(data, objectId, from, to, polyline);
}

} // namespace

bool resolveSegmentEndpoints(const FileData& data, const QString& objectId, double from,
                             double to, QPointF& fromOut, QPointF& toOut)
{
    for (const ObjectData& object : data.objects) {
        if (object.id != objectId)
            continue;
        bool fromFound = false;
        bool toFound = false;
        for (const Keyframe& key : object.keyframes) {
            if (qAbs(key.t - from) <= TimelinePlayer::kTimeEpsilon) {
                fromOut = QPointF(key.x, key.y);
                fromFound = true;
            }
            if (qAbs(key.t - to) <= TimelinePlayer::kTimeEpsilon) {
                toOut = QPointF(key.x, key.y);
                toFound = true;
            }
        }
        return fromFound && toFound;
    }
    return false;
}

// ---- SetPathTypeCommand ----

SetPathTypeCommand::SetPathTypeCommand(QString objectId, double from, double to,
                                       PathType type)
    : m_objectId(std::move(objectId))
    , m_from(from)
    , m_to(to)
    , m_type(type)
{
}

void SetPathTypeCommand::apply(FileData& data)
{
    if (!m_captured) {
        m_oldPaths = data.paths;
        m_captured = true;
    }
    const int index = pathIndexAt(data, m_objectId, m_from, m_to);

    // 先在副本上算出编辑后条目（nextEntry）；nullEntry = 编辑后无条目（直线）
    PathData nextEntry;
    bool hasEntry = true;
    if (m_type == PathType::Arc) {
        if (index < 0) {
            // 直线且无路径点 → 新建条目（单段弧，默认圆心角立即可见）
            nextEntry.objectId = m_objectId;
            nextEntry.from = m_from;
            nextEntry.to = m_to;
            nextEntry.type = PathType::Arc;
            nextEntry.legAngles = { PathGeometry::kDefaultLegAngleDeg };
        } else {
            nextEntry = data.paths.at(index);
            nextEntry.type = PathType::Arc;
            // 直线段角度全零（视觉 = 直线）：切弧线时给默认值，立即可见；
            // 曾是弧线的角度保留（切回再切来不丢编辑成果）
            bool allZero = true;
            for (double angle : nextEntry.legAngles)
                allZero = allZero && !PathGeometry::legIsArc(angle);
            if (allZero) {
                for (double& angle : nextEntry.legAngles)
                    angle = PathGeometry::kDefaultLegAngleDeg;
            }
        }
    } else { // Line
        if (index < 0)
            return; // 已是默认直线：无变更
        nextEntry = data.paths.at(index);
        nextEntry.type = PathType::Line; // 路径点保留（折线），角度保留（切回弧线恢复）
        if (nextEntry.points.isEmpty())
            hasEntry = false; // 零存储默认态：直线且无路径点 → 条目移除
    }
    // 阶段13 任务3：提交前阻挡校验（相交 → 拒绝，数据不变不入栈）
    if (segmentBlockedAfter(data, m_objectId, m_from, m_to,
                            hasEntry ? &nextEntry : nullptr))
        return;
    if (index < 0) {
        if (hasEntry)
            data.paths.append(nextEntry);
    } else if (hasEntry) {
        data.paths[index] = nextEntry;
    } else {
        data.paths.removeAt(index);
    }
}

void SetPathTypeCommand::revert(FileData& data)
{
    data.paths = m_oldPaths;
}

// ---- SetPathLegAngleCommand ----

SetPathLegAngleCommand::SetPathLegAngleCommand(QString objectId, double from, double to,
                                               int legIndex, double angleDeg)
    : m_objectId(std::move(objectId))
    , m_from(from)
    , m_to(to)
    , m_legIndex(legIndex)
    , m_angleDeg(PathGeometry::clampLegAngle(angleDeg))
{
}

void SetPathLegAngleCommand::apply(FileData& data)
{
    if (!m_captured) {
        m_oldPaths = data.paths;
        m_captured = true;
    }
    const PathData* path = findPath(data, m_objectId, m_from, m_to);
    if (!path || m_legIndex < 0 || m_legIndex >= path->legAngles.size())
        return; // 段/段下标不存在：无变更
    PathData nextEntry = *path;
    nextEntry.legAngles[m_legIndex] = m_angleDeg;
    // 阶段13 任务3：提交前阻挡校验
    if (segmentBlockedAfter(data, m_objectId, m_from, m_to, &nextEntry))
        return;
    data.paths[pathIndexAt(data, m_objectId, m_from, m_to)] = nextEntry;
}

void SetPathLegAngleCommand::revert(FileData& data)
{
    data.paths = m_oldPaths;
}

// ---- AddPathPointCommand ----

AddPathPointCommand::AddPathPointCommand(QString objectId, double from, double to,
                                         QPointF scenePos)
    : m_objectId(std::move(objectId))
    , m_from(from)
    , m_to(to)
    , m_scenePos(scenePos)
{
}

void AddPathPointCommand::apply(FileData& data)
{
    if (!m_captured) {
        m_oldPaths = data.paths;
        m_captured = true;
    }
    QPointF fromPos;
    QPointF toPos;
    if (!resolveSegmentEndpoints(data, m_objectId, m_from, m_to, fromPos, toPos))
        return; // 段帧缺失：无变更

    PathData working; // 当前路径的工作副本（无条目 = 默认直线零角度）
    if (const PathData* existing =
            PathGeometry::findPathData(data.paths, m_objectId, m_from, m_to)) {
        working = *existing;
    } else {
        working.objectId = m_objectId;
        working.from = m_from;
        working.to = m_to;
    }
    // 长度不变式归一（防御手工编辑的数据）：legAngles 恒 = points.size()+1
    while (working.legAngles.size() < working.points.size() + 1)
        working.legAngles.append(0.0);
    while (working.legAngles.size() > working.points.size() + 1)
        working.legAngles.removeLast();

    // 点击处投影到当前复合曲线：全局弧长比例 → 所在段 → 段内比例
    const QList<QPointF> polyline = PathGeometry::samplePath(working, fromPos, toPos);
    const double ratio = PathGeometry::projectToRatio(polyline, m_scenePos);
    const double total = PathGeometry::totalLength(polyline);
    if (total <= 0.0)
        return; // 端点重合（畸形）：无变更
    const double target = ratio * total;

    // 定位所在段并求段内弧长比例（逐段采样，与 samplePath 同实现保证一致）
    int legIndex = working.points.size(); // 兜底：末段
    double legRatio = 1.0;
    QPointF pointPos = toPos;
    QPointF legStart = fromPos;
    double cumStart = 0.0;
    for (int i = 0; i <= working.points.size(); ++i) {
        const QPointF legEnd = i < working.points.size()
            ? QPointF(working.points.at(i).x, working.points.at(i).y)
            : toPos;
        const double angle = (working.type == PathType::Arc && i < working.legAngles.size())
            ? working.legAngles.at(i)
            : 0.0;
        const QList<QPointF> leg = PathGeometry::sampleLeg(legStart, legEnd, angle);
        const double legLen = PathGeometry::totalLength(leg);
        if (target <= cumStart + legLen || i == working.points.size()) {
            legIndex = i;
            legRatio = legLen > 0.0
                ? qBound(0.0, (target - cumStart) / legLen, 1.0)
                : 0.0;
            pointPos = PathGeometry::pointAtRatio(
                leg, PathGeometry::cumulativeLengths(leg), legRatio);
            break;
        }
        cumStart += legLen;
        legStart = legEnd;
    }

    // 插入路径点（落在曲线上）+ 所在段圆心角按弧长比例拆分（加点瞬间形状不变）
    working.points.insert(legIndex, PathPoint { pointPos.x(), pointPos.y() });
    const double oldAngle = legIndex < working.legAngles.size()
        ? working.legAngles.at(legIndex)
        : 0.0;
    working.legAngles[legIndex] = legRatio * oldAngle;         // θ₁ = f·θ
    working.legAngles.insert(legIndex + 1, (1.0 - legRatio) * oldAngle); // θ₂ = (1−f)·θ

    // 阶段13 任务3：提交前阻挡校验（相交 → 拒绝）
    if (segmentBlockedAfter(data, m_objectId, m_from, m_to, &working))
        return;

    const int existing = pathIndexAt(data, m_objectId, m_from, m_to);
    if (existing >= 0)
        data.paths[existing] = working;
    else
        data.paths.append(working);
}

void AddPathPointCommand::revert(FileData& data)
{
    data.paths = m_oldPaths;
}

// ---- MovePathPointCommand ----

MovePathPointCommand::MovePathPointCommand(QString objectId, double from, double to,
                                           int pointIndex, QPointF scenePos)
    : m_objectId(std::move(objectId))
    , m_from(from)
    , m_to(to)
    , m_pointIndex(pointIndex)
    , m_scenePos(scenePos)
{
}

void MovePathPointCommand::apply(FileData& data)
{
    if (!m_captured) {
        m_oldPaths = data.paths;
        m_captured = true;
    }
    const PathData* path = findPath(data, m_objectId, m_from, m_to);
    if (!path || m_pointIndex < 0 || m_pointIndex >= path->points.size())
        return; // 路径/点不存在：无变更
    PathData nextEntry = *path;
    nextEntry.points[m_pointIndex] = PathPoint { m_scenePos.x(), m_scenePos.y() };
    // 阶段13 任务3：提交前阻挡校验（拖点落入禁区 → 拒绝，StageView 回弹）
    if (segmentBlockedAfter(data, m_objectId, m_from, m_to, &nextEntry))
        return;
    data.paths[pathIndexAt(data, m_objectId, m_from, m_to)] = nextEntry;
}

void MovePathPointCommand::revert(FileData& data)
{
    data.paths = m_oldPaths;
}

// ---- RemovePathPointCommand ----

RemovePathPointCommand::RemovePathPointCommand(QString objectId, double from, double to,
                                               int pointIndex)
    : m_objectId(std::move(objectId))
    , m_from(from)
    , m_to(to)
    , m_pointIndex(pointIndex)
{
}

void RemovePathPointCommand::apply(FileData& data)
{
    if (!m_captured) {
        m_oldPaths = data.paths;
        m_captured = true;
    }
    const int index = pathIndexAt(data, m_objectId, m_from, m_to);
    if (index < 0)
        return;
    PathData& path = data.paths[index];
    if (m_pointIndex < 0 || m_pointIndex >= path.points.size())
        return;
    path.points.removeAt(m_pointIndex);
    // 相邻两段圆心角合并（加点拆分的逆操作）：θ = θᵢ + θᵢ₊₁
    if (path.legAngles.size() > m_pointIndex + 1) {
        path.legAngles[m_pointIndex] += path.legAngles.at(m_pointIndex + 1);
        path.legAngles.removeAt(m_pointIndex + 1);
    }
    if (path.points.isEmpty() && path.type == PathType::Line)
        data.paths.removeAt(index); // 零存储默认态
}

void RemovePathPointCommand::revert(FileData& data)
{
    data.paths = m_oldPaths;
}

// ---- SetPathPointTCommand ----

SetPathPointTCommand::SetPathPointTCommand(QString objectId, double from, double to,
                                           int pointIndex, double t)
    : m_objectId(std::move(objectId))
    , m_from(from)
    , m_to(to)
    , m_pointIndex(pointIndex)
    , m_t(t)
{
}

void SetPathPointTCommand::apply(FileData& data)
{
    if (!m_captured) {
        m_oldPaths = data.paths;
        m_captured = true;
    }
    PathData* path = findPath(data, m_objectId, m_from, m_to);
    if (!path || m_pointIndex < 0 || m_pointIndex >= path->points.size())
        return;
    QPointF fromPos;
    QPointF toPos;
    if (!resolveSegmentEndpoints(data, m_objectId, m_from, m_to, fromPos, toPos))
        return;

    // 相邻点 t 钳制保序：邻点在当前曲线上的投影弧长比例（点在曲线上，投影即其 t）
    const QList<QPointF> polyline = PathGeometry::samplePath(*path, fromPos, toPos);
    double lo = 0.0;
    double hi = 1.0;
    if (m_pointIndex > 0) {
        const PathPoint& prev = path->points.at(m_pointIndex - 1);
        lo = PathGeometry::projectToRatio(polyline, QPointF(prev.x, prev.y));
    }
    if (m_pointIndex + 1 < path->points.size()) {
        const PathPoint& next = path->points.at(m_pointIndex + 1);
        hi = PathGeometry::projectToRatio(polyline, QPointF(next.x, next.y));
    }
    constexpr double kMargin = 1e-3; // 保序间距（严格在邻点之间）
    const double clamped = qBound(lo + kMargin, m_t, qMax(lo + kMargin, hi - kMargin));
    const QPointF pos = PathGeometry::pointAtRatio(
        polyline, PathGeometry::cumulativeLengths(polyline), clamped);
    PathData nextEntry = *path;
    nextEntry.points[m_pointIndex] = PathPoint { pos.x(), pos.y() };
    // 阶段13 任务3：提交前阻挡校验
    if (segmentBlockedAfter(data, m_objectId, m_from, m_to, &nextEntry))
        return;
    data.paths[pathIndexAt(data, m_objectId, m_from, m_to)] = nextEntry;
}

void SetPathPointTCommand::revert(FileData& data)
{
    data.paths = m_oldPaths;
}

// ---- ResetPathCommand ----

ResetPathCommand::ResetPathCommand(QString objectId, double from, double to)
    : m_objectId(std::move(objectId))
    , m_from(from)
    , m_to(to)
{
}

void ResetPathCommand::apply(FileData& data)
{
    if (!m_captured) {
        m_oldPaths = data.paths;
        m_captured = true;
    }
    const int index = pathIndexAt(data, m_objectId, m_from, m_to);
    if (index >= 0)
        data.paths.removeAt(index);
}

void ResetPathCommand::revert(FileData& data)
{
    data.paths = m_oldPaths;
}
