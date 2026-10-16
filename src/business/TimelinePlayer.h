#pragma once

// 时间轴播放器（业务层）：关键帧插值 + 游标状态 + 播放驱动。
// 插值纯函数：位置直线匀速、朝向按需求角度制做最短弧匀速（差值归一到 [-180,180)
// 后线性）、首帧前/末帧后保持首/末帧值、无关键帧用静态 transform。
// 游标位置/播放状态/倍速是运行时状态，不入存档；播放只发信号驱动显示，
// 绝不修改 FileData。fileDataChanged 后重算当前时刻（撤销/重做关键帧命令时
// 游标处显示立即刷新），并防御时长回退（撤销自动延长命令后游标可能越界）。

#include "data/DataTypes.h"

#include <QElapsedTimer>
#include <QObject>
#include <QString>
#include <QTimer>

class RuntimeData;

class TimelinePlayer : public QObject {
    Q_OBJECT
public:
    explicit TimelinePlayer(RuntimeData* runtimeData, QObject* parent = nullptr);

    // 关键帧时间相等判定容差（秒）：打帧"游标处已有关键帧"与命令 upsert 共用
    static constexpr double kTimeEpsilon = 1e-6;

    // 插值纯函数：给定对象与时刻 T 求当前 Transform。
    // 关键帧列表按 t 有序由命令/入口维护；此处对乱序输入自行防御（排序副本）。
    static Transform interpolate(const ObjectData& object, double t);
    // 阶段12：带路径插值——段内存在 PathData 时位置沿复合曲线弧长匀速
    // （t 线性 → 弧长线性，采样折线化见 PathGeometry.h）；无 PathData 走原直线
    // 路径（结果与旧签名逐位一致）。朝向与路径无关，保持最短弧匀速。
    static Transform interpolate(const ObjectData& object, const QList<PathData>& paths,
                                 double t);

    // ---- 游标 ----
    double currentTime() const { return m_currentTime; }
    double duration() const;
    void seekTo(double t); // 收敛到 [0, duration]；变化时发 currentTimeChanged

    // ---- 播放 / 暂停 / 倍速 ----
    void play();
    void pause();
    bool isPlaying() const { return m_playing; }
    double playbackSpeed() const { return m_speed; }
    void setSpeed(double speed); // 负值/零忽略

    // 当前时刻的显示值（插值结果）；对象不存在时返回 false
    bool valueAt(const QString& objectId, Transform& out) const;

signals:
    void currentTimeChanged(double t); // 游标移动、播放 tick、fileDataChanged 重算后发出
    void playingChanged(bool playing);
    void speedChanged(double speed);

private:
    void onTick();
    void onFileDataChanged(); // 钳制游标并重算当前时刻（撤销/重做立即刷新显示）
    void onFileReplaced();    // 新建/打开：停止播放、游标归零
    void setPlaying(bool playing);

    RuntimeData* m_runtimeData = nullptr; // 不拥有
    double m_currentTime = 0.0;
    double m_speed = 1.0;
    bool m_playing = false;
    QElapsedTimer m_elapsed; // 播放中按真实流逝时间 × 倍速推进
    QTimer m_timer;          // 60fps tick（约 16ms）
};
