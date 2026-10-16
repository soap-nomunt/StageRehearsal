#include "TimelinePlayer.h"

#include "PathGeometry.h"
#include "data/RuntimeData.h"

#include <algorithm>
#include <cmath>

namespace {

// tick 间隔 16ms ≈ 60fps：兼顾顺滑与刷新开销（场景重建不在 tick 路径上，
// 每次 tick 只做图元 setPos/setRotation 与必要的重绘）
constexpr int kTickIntervalMs = 16;

Transform keyframeValue(const Keyframe& key)
{
    return { key.x, key.y, key.rotation };
}

// 需求角度制最短弧差值：归一化到 [-180, 180)，如 350°→10° 得 +20° 而非 -340°
double shortestArcDelta(double from, double to)
{
    double delta = std::fmod(to - from, 360.0);
    if (delta >= 180.0)
        delta -= 360.0;
    else if (delta < -180.0)
        delta += 360.0;
    return delta;
}

double normalizedDegrees(double degrees)
{
    degrees = std::fmod(degrees, 360.0);
    return degrees < 0.0 ? degrees + 360.0 : degrees;
}

} // namespace

TimelinePlayer::TimelinePlayer(RuntimeData* runtimeData, QObject* parent)
    : QObject(parent)
    , m_runtimeData(runtimeData)
{
    m_timer.setInterval(kTickIntervalMs);
    connect(&m_timer, &QTimer::timeout, this, &TimelinePlayer::onTick);
    connect(m_runtimeData, &RuntimeData::fileDataChanged,
            this, &TimelinePlayer::onFileDataChanged);
    connect(m_runtimeData, &RuntimeData::fileReplaced,
            this, &TimelinePlayer::onFileReplaced);
}

Transform TimelinePlayer::interpolate(const ObjectData& object, double t)
{
    return interpolate(object, {}, t); // 无路径：直线匀速（阶段12 前行为逐位一致）
}

Transform TimelinePlayer::interpolate(const ObjectData& object, const QList<PathData>& paths,
                                      double t)
{
    if (object.keyframes.isEmpty())
        return object.transform; // 无关键帧：静态 transform

    // 关键帧列表按 t 有序由命令/入口维护；对乱序输入（如手工编辑的存档）自行防御
    QList<Keyframe> keys = object.keyframes;
    std::sort(keys.begin(), keys.end(),
              [](const Keyframe& a, const Keyframe& b) { return a.t < b.t; });

    // 首帧前/末帧后保持首/末帧值（需求第 21 条）；单帧任意时刻取该帧
    if (t <= keys.first().t)
        return keyframeValue(keys.first());
    if (t >= keys.last().t)
        return keyframeValue(keys.last());

    // 定位区间 [i, i+1]：keys[i].t <= t <= keys[i+1].t
    int next = 1;
    while (next < keys.size() && keys.at(next).t < t)
        ++next;
    const Keyframe& a = keys.at(next - 1);
    const Keyframe& b = keys.at(next);
    const double span = b.t - a.t;
    if (span <= 0.0)
        return keyframeValue(b); // 同时刻重复帧（畸形数据）：取后者，避免除零

    const double ratio = (t - a.t) / span;
    Transform result;
    // 位置：段内有 PathData → 复合曲线弧长匀速（t 线性 → 弧长线性）；
    // 否则直线匀速（与阶段12 前逐位一致）
    if (const PathData* path = PathGeometry::findPathData(paths, object.id, a.t, b.t)) {
        const QList<QPointF> polyline =
            PathGeometry::samplePath(*path, QPointF(a.x, a.y), QPointF(b.x, b.y));
        const QList<double> cum = PathGeometry::cumulativeLengths(polyline);
        const QPointF pos = PathGeometry::pointAtRatio(polyline, cum, ratio);
        result.x = pos.x();
        result.y = pos.y();
    } else {
        result.x = a.x + (b.x - a.x) * ratio; // 位置直线匀速
        result.y = a.y + (b.y - a.y) * ratio;
    }
    // 朝向最短弧匀速，结果归一化到 [0,360)
    result.rotation =
        normalizedDegrees(a.rotation + shortestArcDelta(a.rotation, b.rotation) * ratio);
    return result;
}

double TimelinePlayer::duration() const
{
    return m_runtimeData->fileData().timeline.duration;
}

void TimelinePlayer::seekTo(double t)
{
    const double clamped = qBound(0.0, t, duration());
    if (clamped == m_currentTime)
        return;
    m_currentTime = clamped;
    emit currentTimeChanged(m_currentTime);
    if (m_playing && m_currentTime >= duration()) {
        // 到末尾自动停止：游标停在末帧，便于看清终态（不循环——排练回放
        // 需要明确的结束点，循环易与起点混淆）
        pause();
    }
}

void TimelinePlayer::play()
{
    if (m_playing)
        return;
    if (m_currentTime >= duration()) {
        // 停在末尾时再次播放：从头开始
        m_currentTime = 0.0;
        emit currentTimeChanged(m_currentTime);
    }
    m_elapsed.start();
    m_timer.start();
    setPlaying(true);
}

void TimelinePlayer::pause()
{
    if (!m_playing)
        return;
    m_timer.stop();
    setPlaying(false);
}

void TimelinePlayer::setSpeed(double speed)
{
    if (speed <= 0.0 || speed == m_speed)
        return;
    m_speed = speed;
    emit speedChanged(m_speed);
}

bool TimelinePlayer::valueAt(const QString& objectId, Transform& out) const
{
    for (const ObjectData& object : m_runtimeData->fileData().objects) {
        if (object.id == objectId) {
            out = interpolate(object, m_currentTime);
            return true;
        }
    }
    return false;
}

void TimelinePlayer::onTick()
{
    // 真实流逝时间 × 倍速推进：tick 抖动不累积误差
    const double advance = m_elapsed.restart() / 1000.0 * m_speed;
    seekTo(m_currentTime + advance); // seekTo 内部负责钳制、到末尾暂停与信号
}

void TimelinePlayer::onFileDataChanged()
{
    // 时长可能因撤销"自动延长"命令而回退：钳制游标
    const double clamped = qBound(0.0, m_currentTime, duration());
    if (clamped != m_currentTime)
        m_currentTime = clamped;
    // 关键帧数据可能已变（撤销/重做）：即使游标数值不变也发信号，
    // 让视口/Inspector 立即按当前时刻重算显示值
    emit currentTimeChanged(m_currentTime);
}

void TimelinePlayer::onFileReplaced()
{
    pause();
    m_currentTime = 0.0;
    emit currentTimeChanged(m_currentTime);
}

void TimelinePlayer::setPlaying(bool playing)
{
    if (m_playing == playing)
        return;
    m_playing = playing;
    emit playingChanged(m_playing);
}
