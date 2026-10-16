#pragma once

// 时间轴命令（阶段5）：关键帧 upsert/删除/拖动改时刻，场次标签添加/改名/删除，
// 以及时长自动延长。与既有编辑命令共用同一撤销栈（EditorCore::executeCommand）。
// 时间轴交互完善追加：批量改时刻（方向键步进）、场次标签移动改时刻。
//
// 约定与 Commands.h 一致：apply 首次执行时从 FileData 捕获旧值供 revert
// （对 redo 幂等）。关键帧/标签命令的捕获粒度是"被触及对象的整条关键帧列表
// （+ 旧时长）/整个标签列表"——列表很小，整体快照换来严格互逆与排序不变式。
//
// 时长自动延长（需求：关键帧/标签落点超出 duration 时自动延长）归属命令内部：
// 同一命令里改 duration，撤销时一并还原，保证 apply/revert 严格互逆。

#include "TimelinePlayer.h" // kTimeEpsilon
#include "data/DataTypes.h"

#include "Commands.h"

#include <QList>
#include <QPair>
#include <QString>

// 单条关键帧写入目标（对象 id + 新关键帧）
struct ObjectKeyframe {
    QString id;
    Keyframe keyframe;
};

// 单条关键帧引用（对象 id + 时刻），删除用
struct ObjectKeyframeRef {
    QString id;
    double t = 0.0;
};

// 打帧/更新（upsert）：T 处已有关键帧（|Δt| <= kTimeEpsilon）→ 更新其值；
// 否则插入并保持按 t 有序。一次拖拽/一次打帧按钮可能涉及多个对象，
// 批量合并为一条命令 = 一步撤销。
// 阶段12 级联：插入新帧（非更新）落进某段区间内部 → 该段被劈开，其 PathData
// 静默丢弃（同命令快照 paths，撤销/重做严格互逆）；更新既有帧（端点编辑）不动路径。
class UpsertKeyframesCommand : public Command {
public:
    explicit UpsertKeyframesCommand(QList<ObjectKeyframe> items);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    QList<ObjectKeyframe> m_items;
    QList<QPair<QString, QList<Keyframe>>> m_oldKeyframes; // (对象 id, 旧列表)
    QList<PathData> m_oldPaths; // 阶段12：级联丢弃的路径快照（整条列表）
    double m_oldDuration = 0.0;
    bool m_captured = false;
};

// 删除关键帧：按 (id, t) 批量。不收缩时长（时长只自动延长）。
// 阶段12 级联：被删帧作为端点的相邻两段（from≈t 或 to≈t）PathData 一并丢弃。
class RemoveKeyframesCommand : public Command {
public:
    explicit RemoveKeyframesCommand(QList<ObjectKeyframeRef> refs);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    QList<ObjectKeyframeRef> m_refs;
    QList<QPair<QString, QList<Keyframe>>> m_oldKeyframes;
    QList<PathData> m_oldPaths; // 阶段12
    bool m_captured = false;
};

// 拖动关键帧改时刻：t 变化、值不变；目标时刻已有帧则被覆盖（upsert 语义，
// 捕获的旧列表保证 revert 还原双方）。
// 阶段12 级联：段标识跟随重绑定——该对象路径的 from/to ≈ oldT 者改为 newT。
class MoveKeyframeCommand : public Command {
public:
    MoveKeyframeCommand(QString id, double oldT, double newT);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    QString m_id;
    double m_oldT;
    double m_newT;
    QList<Keyframe> m_oldKeyframes;
    QList<PathData> m_oldPaths; // 阶段12
    double m_oldDuration = 0.0;
    bool m_captured = false;
};

// 批量改时刻（方向键步进）：多条 (id, t) 统一平移 deltaT，一条命令一步撤销。
// 逐帧 clamp t≥0；同对象目标时刻已有帧按 upsert 覆盖（捕获的旧列表保证 revert
// 还原双方）；同批两帧钳到同一时刻时按列表顺序后者覆盖前者（逐帧顺序处理的
// 自然结果）；时长自动延长沿用。
// 阶段12 级联：同 MoveKeyframeCommand，逐条重绑定 from/to（≈ 旧时刻 → 钳后新时刻）。
class MoveKeyframesCommand : public Command {
public:
    MoveKeyframesCommand(QList<ObjectKeyframeRef> refs, double deltaT);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    QList<ObjectKeyframeRef> m_refs;
    double m_deltaT;
    QList<QPair<QString, QList<Keyframe>>> m_oldKeyframes; // (对象 id, 旧列表)
    QList<PathData> m_oldPaths; // 阶段12
    double m_oldDuration = 0.0;
    bool m_captured = false;
};

// 添加场次标签（纯视觉标记）：插入并保持按 t 有序；超时长自动延长。
class AddSceneLabelCommand : public Command {
public:
    explicit AddSceneLabelCommand(SceneLabel label);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    SceneLabel m_label;
    QList<SceneLabel> m_oldLabels;
    double m_oldDuration = 0.0;
    bool m_captured = false;
};

// 场次标签改名：按下标定位（撤销/重做对称，下标在 apply/revert 间稳定）。
class RenameSceneLabelCommand : public Command {
public:
    RenameSceneLabelCommand(int index, QString newName);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    int m_index;
    QString m_newName;
    QString m_oldName;
    bool m_captured = false;
};

// 移动场次标签时刻：按下标改 t、名不变，改后按 t 重排保持有序（下标在
// apply/revert 间稳定：revert 还原整个列表，redo 重放同一改动，严格互逆）；
// 超时长自动延长。
class MoveSceneLabelCommand : public Command {
public:
    MoveSceneLabelCommand(int index, double newT);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    int m_index;
    double m_newT;
    QList<SceneLabel> m_oldLabels;
    double m_oldDuration = 0.0;
    bool m_captured = false;
};

// 删除场次标签：按下标；revert 原位还原整个标签列表。
class RemoveSceneLabelCommand : public Command {
public:
    explicit RemoveSceneLabelCommand(int index);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    int m_index;
    QList<SceneLabel> m_oldLabels;
    bool m_captured = false;
};
