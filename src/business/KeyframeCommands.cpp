#include "KeyframeCommands.h"

#include "PathGeometry.h"

#include <QPointF>

#include <algorithm>

namespace {

ObjectData* findObject(FileData& data, const QString& id)
{
    for (ObjectData& object : data.objects) {
        if (object.id == id)
            return &object;
    }
    return nullptr;
}

// T 处（容差内）已有关键帧的下标，无则 -1；列表按 t 有序由本文件命令维护
int keyframeIndexAt(const QList<Keyframe>& keys, double t)
{
    for (int i = 0; i < keys.size(); ++i) {
        if (qAbs(keys.at(i).t - t) <= TimelinePlayer::kTimeEpsilon)
            return i;
    }
    return -1;
}

// 按 t 有序插入（相等时刻插到已有帧之后，保持插入顺序稳定）
void insertKeyframeSorted(QList<Keyframe>& keys, const Keyframe& keyframe)
{
    int pos = keys.size();
    while (pos > 0 && keys.at(pos - 1).t > keyframe.t)
        --pos;
    keys.insert(pos, keyframe);
}

void upsertKeyframe(QList<Keyframe>& keys, const Keyframe& keyframe)
{
    const int index = keyframeIndexAt(keys, keyframe.t);
    if (index >= 0)
        keys[index] = keyframe; // 已有：更新（位置不变，时刻不变）
    else
        insertKeyframeSorted(keys, keyframe);
}

// 时长自动延长：落点超出 duration 时随同一命令延长（revert 由捕获的旧时长还原）
void ensureDuration(FileData& data, double t)
{
    if (t > data.timeline.duration)
        data.timeline.duration = t;
}

// ---- 阶段12：路径级联（与关键帧改动同命令快照，撤销/重做严格互逆）----

// 段内打帧（插入新帧，非更新端点）→ 段被劈开，该段 PathData 静默丢弃
void dropPathsContaining(FileData& data, const QString& objectId, double t)
{
    for (int i = data.paths.size() - 1; i >= 0; --i) {
        const PathData& path = data.paths.at(i);
        if (path.objectId == objectId
            && path.from + TimelinePlayer::kTimeEpsilon < t
            && t < path.to - TimelinePlayer::kTimeEpsilon)
            data.paths.removeAt(i);
    }
}

// 删除端点帧 → 相邻两段（from≈t 或 to≈t）PathData 丢弃
void dropPathsAtEndpoint(FileData& data, const QString& objectId, double t)
{
    for (int i = data.paths.size() - 1; i >= 0; --i) {
        const PathData& path = data.paths.at(i);
        if (path.objectId == objectId
            && (qAbs(path.from - t) <= TimelinePlayer::kTimeEpsilon
                || qAbs(path.to - t) <= TimelinePlayer::kTimeEpsilon))
            data.paths.removeAt(i);
    }
}

// 帧改时刻 → 段标识跟随重绑定（from/to ≈ oldT 者改为 newT）
void rebindPaths(FileData& data, const QString& objectId, double oldT, double newT)
{
    for (PathData& path : data.paths) {
        if (path.objectId != objectId)
            continue;
        if (qAbs(path.from - oldT) <= TimelinePlayer::kTimeEpsilon)
            path.from = newT;
        if (qAbs(path.to - oldT) <= TimelinePlayer::kTimeEpsilon)
            path.to = newT;
    }
}

// ---- 阶段13 修复：对象位置变更（拖拽/微调/Inspector/打帧/粘贴关键帧）的阻挡校验 ----
// 统一收口在 UpsertKeyframesCommand：所有位置类关键帧写入都经过它。
// 对位置实际变化（与既有帧值/插入点插值基准不同；纯旋转或值未变跳过）的条目，
// 在新状态上计算受影响相邻段（更新帧：前一帧→该帧、该帧→后一帧；插入劈段：
// 劈出的两条新段；首前/末后插入：仅一侧段），逐段按实际路径（有 PathData 用复合
// 曲线，无则直线）扫掠判定（PathGeometry::pathBlocked 同一采样实现）。

// 把 upsert 条目应用到 data（含段内劈开丢弃路径级联与时长延长；不含阻挡校验）
void applyUpsertItems(FileData& data, const QList<ObjectKeyframe>& items)
{
    for (const ObjectKeyframe& item : items) {
        if (ObjectData* object = findObject(data, item.id)) {
            const bool isInsert =
                keyframeIndexAt(object->keyframes, item.keyframe.t) < 0;
            if (isInsert)
                dropPathsContaining(data, item.id, item.keyframe.t);
            upsertKeyframe(object->keyframes, item.keyframe);
        }
        ensureDuration(data, item.keyframe.t);
    }
}

// 应用后校验：data = 已应用条目后的状态；oldKeyframes/oldPaths = 应用前快照
bool editsBlockedPostApply(const FileData& data,
                           const QList<QPair<QString, QList<Keyframe>>>& oldKeyframes,
                           const QList<PathData>& oldPaths,
                           const QList<ObjectKeyframe>& items)
{
    for (const ObjectKeyframe& item : items) {
        const ObjectData* object = findObject(const_cast<FileData&>(data), item.id);
        if (!object)
            continue;
        // 位置是否实际变化：与既有帧值或插入点插值基准比较（纯旋转/值未变跳过）
        bool moved = false;
        QList<Keyframe> oldList;
        for (const auto& entry : oldKeyframes) {
            if (entry.first == item.id)
                oldList = entry.second;
        }
        const int oldIndex = keyframeIndexAt(oldList, item.keyframe.t);
        if (oldIndex >= 0) {
            const Keyframe& oldKey = oldList.at(oldIndex);
            moved = qAbs(oldKey.x - item.keyframe.x) > 1e-9
                || qAbs(oldKey.y - item.keyframe.y) > 1e-9;
        } else {
            // 插入新帧：基准 = 旧状态该时刻插值位置（轨迹不变则不算移动）
            ObjectData baseline = *object;
            baseline.keyframes = oldList;
            const Transform base =
                TimelinePlayer::interpolate(baseline, oldPaths, item.keyframe.t);
            moved = qAbs(base.x - item.keyframe.x) > 1e-9
                || qAbs(base.y - item.keyframe.y) > 1e-9;
        }
        if (!moved)
            continue;
        // 受影响相邻段（新状态的关键帧列表按 t 有序）
        const QList<Keyframe>& keys = object->keyframes;
        int index = keyframeIndexAt(keys, item.keyframe.t);
        if (index < 0)
            continue;
        for (int side = 0; side < 2; ++side) {
            const bool prevSide = side == 0;
            const int neighbor = prevSide ? index - 1 : index + 1;
            if (neighbor < 0 || neighbor >= keys.size())
                continue; // 首帧前/末帧后插入：只有一侧段
            const double segFrom =
                prevSide ? keys.at(neighbor).t : item.keyframe.t;
            const double segTo = prevSide ? item.keyframe.t : keys.at(neighbor).t;
            if (segTo - segFrom <= TimelinePlayer::kTimeEpsilon)
                continue;
            const Keyframe& a = prevSide ? keys.at(neighbor) : keys.at(index);
            const Keyframe& b = prevSide ? keys.at(index) : keys.at(neighbor);
            const QPointF fromPos(a.x, a.y);
            const QPointF toPos(b.x, b.y);
            QList<QPointF> polyline;
            if (const PathData* path = PathGeometry::findPathData(data.paths, item.id,
                                                                  segFrom, segTo)) {
                polyline = PathGeometry::samplePath(*path, fromPos, toPos);
            } else {
                polyline = { fromPos, toPos };
            }
            if (PathGeometry::pathBlocked(data, item.id, segFrom, segTo, polyline))
                return true;
        }
    }
    return false;
}

} // namespace

// 阶段13 修复：预览/入口共用的位置变更阻挡查询——把条目模拟应用到 data 副本后
// 按受影响段扫掠判定（与 UpsertKeyframesCommand::apply 同路径，不改真实数据）
bool keyframeEditsBlocked(const FileData& data, const QList<ObjectKeyframe>& items)
{
    FileData working = data;
    // 旧快照（用于位置变化检测）
    QList<QPair<QString, QList<Keyframe>>> oldKeyframes;
    for (const ObjectKeyframe& item : items) {
        bool already = false;
        for (const auto& entry : oldKeyframes)
            already = already || entry.first == item.id;
        if (!already) {
            const ObjectData* object = findObject(working, item.id);
            if (object)
                oldKeyframes.append(qMakePair(item.id, object->keyframes));
        }
    }
    const QList<PathData> oldPaths = working.paths;
    applyUpsertItems(working, items);
    return editsBlockedPostApply(working, oldKeyframes, oldPaths, items);
}

// ---- UpsertKeyframesCommand ----

UpsertKeyframesCommand::UpsertKeyframesCommand(QList<ObjectKeyframe> items)
    : m_items(std::move(items))
{
}

void UpsertKeyframesCommand::apply(FileData& data)
{
    if (!m_captured) {
        for (const ObjectKeyframe& item : m_items) {
            const QString& id = item.id;
            bool already = false;
            for (const auto& entry : m_oldKeyframes)
                already = already || entry.first == id;
            if (!already) {
                const ObjectData* object = findObject(data, id);
                m_oldKeyframes.append(
                    qMakePair(id, object ? object->keyframes : QList<Keyframe>()));
            }
        }
        m_oldDuration = data.timeline.duration;
        m_oldPaths = data.paths;
        m_captured = true;
    }
    applyUpsertItems(data, m_items); // 含段内劈开丢弃路径级联与时长延长
    // 阶段13 修复：位置变更的阻挡校验——穿阻挡则整条命令回滚（数据不变 →
    // executeCommand 无变更丢弃，调用方得 false、零撤销条目）
    if (editsBlockedPostApply(data, m_oldKeyframes, m_oldPaths, m_items))
        revert(data);
}

void UpsertKeyframesCommand::revert(FileData& data)
{
    for (const auto& entry : m_oldKeyframes) {
        if (ObjectData* object = findObject(data, entry.first))
            object->keyframes = entry.second;
    }
    data.timeline.duration = m_oldDuration;
    data.paths = m_oldPaths;
}

// ---- RemoveKeyframesCommand ----

RemoveKeyframesCommand::RemoveKeyframesCommand(QList<ObjectKeyframeRef> refs)
    : m_refs(std::move(refs))
{
}

void RemoveKeyframesCommand::apply(FileData& data)
{
    if (!m_captured) {
        for (const ObjectKeyframeRef& ref : m_refs) {
            bool already = false;
            for (const auto& entry : m_oldKeyframes)
                already = already || entry.first == ref.id;
            if (!already) {
                const ObjectData* object = findObject(data, ref.id);
                m_oldKeyframes.append(
                    qMakePair(ref.id, object ? object->keyframes : QList<Keyframe>()));
            }
        }
        m_oldPaths = data.paths;
        m_captured = true;
    }
    for (const ObjectKeyframeRef& ref : m_refs) {
        ObjectData* object = findObject(data, ref.id);
        if (!object)
            continue;
        const int index = keyframeIndexAt(object->keyframes, ref.t);
        if (index >= 0) {
            object->keyframes.removeAt(index);
            dropPathsAtEndpoint(data, ref.id, ref.t); // 阶段12：相邻两段路径丢弃
        }
    }
}

void RemoveKeyframesCommand::revert(FileData& data)
{
    for (const auto& entry : m_oldKeyframes) {
        if (ObjectData* object = findObject(data, entry.first))
            object->keyframes = entry.second;
    }
    data.paths = m_oldPaths;
}

// ---- MoveKeyframeCommand ----

MoveKeyframeCommand::MoveKeyframeCommand(QString id, double oldT, double newT)
    : m_id(std::move(id))
    , m_oldT(oldT)
    , m_newT(newT)
{
}

void MoveKeyframeCommand::apply(FileData& data)
{
    ObjectData* object = findObject(data, m_id);
    if (!object)
        return;
    if (!m_captured) {
        m_oldKeyframes = object->keyframes;
        m_oldDuration = data.timeline.duration;
        m_oldPaths = data.paths;
        m_captured = true;
    }
    const int index = keyframeIndexAt(object->keyframes, m_oldT);
    if (index < 0)
        return;
    Keyframe moved = object->keyframes.at(index);
    moved.t = m_newT;
    object->keyframes.removeAt(index);
    upsertKeyframe(object->keyframes, moved); // 目标时刻已有帧 → 覆盖
    rebindPaths(data, m_id, m_oldT, m_newT);  // 阶段12：段标识跟随重绑定
    ensureDuration(data, m_newT);
}

void MoveKeyframeCommand::revert(FileData& data)
{
    if (ObjectData* object = findObject(data, m_id))
        object->keyframes = m_oldKeyframes;
    data.timeline.duration = m_oldDuration;
    data.paths = m_oldPaths;
}

// ---- MoveKeyframesCommand ----

MoveKeyframesCommand::MoveKeyframesCommand(QList<ObjectKeyframeRef> refs, double deltaT)
    : m_refs(std::move(refs))
    , m_deltaT(deltaT)
{
}

void MoveKeyframesCommand::apply(FileData& data)
{
    if (!m_captured) {
        for (const ObjectKeyframeRef& ref : m_refs) {
            bool already = false;
            for (const auto& entry : m_oldKeyframes)
                already = already || entry.first == ref.id;
            if (!already) {
                const ObjectData* object = findObject(data, ref.id);
                m_oldKeyframes.append(
                    qMakePair(ref.id, object ? object->keyframes : QList<Keyframe>()));
            }
        }
        m_oldDuration = data.timeline.duration;
        m_oldPaths = data.paths;
        m_captured = true;
    }
    // 逐帧顺序处理：同批两帧钳到同一时刻（如同遭 clamp 到 0）时列表靠后者的
    // upsert 覆盖靠前者；源时刻找不到帧（被同批前序移动覆盖）则跳过该条
    for (const ObjectKeyframeRef& ref : m_refs) {
        ObjectData* object = findObject(data, ref.id);
        if (!object)
            continue;
        const int index = keyframeIndexAt(object->keyframes, ref.t);
        if (index < 0)
            continue;
        Keyframe moved = object->keyframes.at(index);
        moved.t = qMax(0.0, ref.t + m_deltaT);
        object->keyframes.removeAt(index);
        upsertKeyframe(object->keyframes, moved); // 目标时刻已有帧 → 覆盖
        rebindPaths(data, ref.id, ref.t, moved.t); // 阶段12：段标识跟随重绑定
        ensureDuration(data, moved.t);
    }
}

void MoveKeyframesCommand::revert(FileData& data)
{
    for (const auto& entry : m_oldKeyframes) {
        if (ObjectData* object = findObject(data, entry.first))
            object->keyframes = entry.second;
    }
    data.timeline.duration = m_oldDuration;
    data.paths = m_oldPaths;
}

// ---- AddSceneLabelCommand ----

AddSceneLabelCommand::AddSceneLabelCommand(SceneLabel label)
    : m_label(std::move(label))
{
}

void AddSceneLabelCommand::apply(FileData& data)
{
    if (!m_captured) {
        m_oldLabels = data.timeline.labels;
        m_oldDuration = data.timeline.duration;
        m_captured = true;
    }
    int pos = data.timeline.labels.size();
    while (pos > 0 && data.timeline.labels.at(pos - 1).t > m_label.t)
        --pos;
    data.timeline.labels.insert(pos, m_label);
    ensureDuration(data, m_label.t);
}

void AddSceneLabelCommand::revert(FileData& data)
{
    data.timeline.labels = m_oldLabels;
    data.timeline.duration = m_oldDuration;
}

// ---- RenameSceneLabelCommand ----

RenameSceneLabelCommand::RenameSceneLabelCommand(int index, QString newName)
    : m_index(index)
    , m_newName(std::move(newName))
{
}

void RenameSceneLabelCommand::apply(FileData& data)
{
    if (m_index < 0 || m_index >= data.timeline.labels.size())
        return;
    if (!m_captured) {
        m_oldName = data.timeline.labels.at(m_index).name;
        m_captured = true;
    }
    data.timeline.labels[m_index].name = m_newName;
}

void RenameSceneLabelCommand::revert(FileData& data)
{
    if (m_index >= 0 && m_index < data.timeline.labels.size())
        data.timeline.labels[m_index].name = m_oldName;
}

// ---- MoveSceneLabelCommand ----

MoveSceneLabelCommand::MoveSceneLabelCommand(int index, double newT)
    : m_index(index)
    , m_newT(newT)
{
}

void MoveSceneLabelCommand::apply(FileData& data)
{
    if (m_index < 0 || m_index >= data.timeline.labels.size())
        return;
    if (!m_captured) {
        m_oldLabels = data.timeline.labels;
        m_oldDuration = data.timeline.duration;
        m_captured = true;
    }
    data.timeline.labels[m_index].t = m_newT;
    // 按 t 重排保持有序不变式；revert 还原整个列表，故 apply/revert 严格互逆
    std::stable_sort(data.timeline.labels.begin(), data.timeline.labels.end(),
                     [](const SceneLabel& a, const SceneLabel& b) { return a.t < b.t; });
    ensureDuration(data, m_newT);
}

void MoveSceneLabelCommand::revert(FileData& data)
{
    data.timeline.labels = m_oldLabels;
    data.timeline.duration = m_oldDuration;
}

// ---- RemoveSceneLabelCommand ----

RemoveSceneLabelCommand::RemoveSceneLabelCommand(int index)
    : m_index(index)
{
}

void RemoveSceneLabelCommand::apply(FileData& data)
{
    if (m_index < 0 || m_index >= data.timeline.labels.size())
        return;
    if (!m_captured) {
        m_oldLabels = data.timeline.labels;
        m_captured = true;
    }
    data.timeline.labels.removeAt(m_index);
}

void RemoveSceneLabelCommand::revert(FileData& data)
{
    data.timeline.labels = m_oldLabels;
}
