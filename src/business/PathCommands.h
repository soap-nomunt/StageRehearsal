#pragma once

// 路径命令（阶段12）：关键帧间移动路径的一切数据修改载体，入撤销栈。
// 快照粒度沿用"整条列表"先例：apply 首次执行时捕获 FileData.paths 整体，
// revert 原样写回（对 redo 幂等，严格互逆）。
// 零存储默认态不变式由本文件命令维护：type=Line 且 points 为空的段不存在
// PathData（恢复直线/删空路径点后整个条目移除）。
// 关键帧级联（moveKeyframe 段标识重绑定 / 段内 upsert 打帧丢弃 / removeKeyframes
// 相邻两段丢弃 / 删除对象级联）实现在既有关键帧/对象命令内部，与帧改动同快照。

#include "Commands.h"
#include "PathGeometry.h"
#include "TimelinePlayer.h"
#include "data/DataTypes.h"

#include <QList>
#include <QPointF>
#include <QString>

// 段内公共工具：解析端点（两帧数据值），帧缺失返回 false
bool resolveSegmentEndpoints(const FileData& data, const QString& objectId, double from,
                             double to, QPointF& fromOut, QPointF& toOut);

// 改连线类型（直线 ⇄ 弧线）。切弧线时初始化各段默认圆心角（30°，立即可见）；
// 切回直线保留路径点（折线路径）与角度（再切回弧线时恢复）；直线且无路径点
// → 条目移除（零存储默认态）。
class SetPathTypeCommand : public Command {
public:
    SetPathTypeCommand(QString objectId, double from, double to, PathType type);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    QString m_objectId;
    double m_from;
    double m_to;
    PathType m_type;
    QList<PathData> m_oldPaths;
    bool m_captured = false;
};

// 改某段圆心角（±350° 钳制；|θ|<0.1° 由几何层按直线处理）
class SetPathLegAngleCommand : public Command {
public:
    SetPathLegAngleCommand(QString objectId, double from, double to, int legIndex,
                           double angleDeg);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    QString m_objectId;
    double m_from;
    double m_to;
    int m_legIndex;
    double m_angleDeg;
    QList<PathData> m_oldPaths;
    bool m_captured = false;
};

// 添加路径点：点击处投影到当前复合曲线（点吸附落在曲线上），所在段圆心角按
// 弧长比例拆分（θ₁=f·θ，θ₂=(1−f)·θ）——加点瞬间路径形状不变。
class AddPathPointCommand : public Command {
public:
    AddPathPointCommand(QString objectId, double from, double to, QPointF scenePos);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    QString m_objectId;
    double m_from;
    double m_to;
    QPointF m_scenePos;
    QList<PathData> m_oldPaths;
    bool m_captured = false;
};

// 拖动路径点（含方向键微调）：吸附/步进由调用方完成，命令只写新位置
class MovePathPointCommand : public Command {
public:
    MovePathPointCommand(QString objectId, double from, double to, int pointIndex,
                         QPointF scenePos);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    QString m_objectId;
    double m_from;
    double m_to;
    int m_pointIndex;
    QPointF m_scenePos;
    QList<PathData> m_oldPaths;
    bool m_captured = false;
};

// 删除路径点：相邻两段圆心角合并（θ = θᵢ + θᵢ₊₁，拆分保形的逆操作）；
// 删空路径点且类型为直线 → 条目移除（零存储默认态）
class RemovePathPointCommand : public Command {
public:
    RemovePathPointCommand(QString objectId, double from, double to, int pointIndex);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    QString m_objectId;
    double m_from;
    double m_to;
    int m_pointIndex;
    QList<PathData> m_oldPaths;
    bool m_captured = false;
};

// 改路径点 t（沿路径弧长百分比）：点沿当前曲线滑动重定位；相邻点 t 钳制保序
class SetPathPointTCommand : public Command {
public:
    SetPathPointTCommand(QString objectId, double from, double to, int pointIndex,
                         double t);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    QString m_objectId;
    double m_from;
    double m_to;
    int m_pointIndex;
    double m_t;
    QList<PathData> m_oldPaths;
    bool m_captured = false;
};

// 恢复直线：整个 PathData 条目移除（零存储默认态）
class ResetPathCommand : public Command {
public:
    ResetPathCommand(QString objectId, double from, double to);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    QString m_objectId;
    double m_from;
    double m_to;
    QList<PathData> m_oldPaths;
    bool m_captured = false;
};
