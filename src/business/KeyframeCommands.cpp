#include "KeyframeCommands.h"

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

} // namespace

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
        m_captured = true;
    }
    for (const ObjectKeyframe& item : m_items) {
        if (ObjectData* object = findObject(data, item.id))
            upsertKeyframe(object->keyframes, item.keyframe);
        ensureDuration(data, item.keyframe.t);
    }
}

void UpsertKeyframesCommand::revert(FileData& data)
{
    for (const auto& entry : m_oldKeyframes) {
        if (ObjectData* object = findObject(data, entry.first))
            object->keyframes = entry.second;
    }
    data.timeline.duration = m_oldDuration;
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
        m_captured = true;
    }
    for (const ObjectKeyframeRef& ref : m_refs) {
        ObjectData* object = findObject(data, ref.id);
        if (!object)
            continue;
        const int index = keyframeIndexAt(object->keyframes, ref.t);
        if (index >= 0)
            object->keyframes.removeAt(index);
    }
}

void RemoveKeyframesCommand::revert(FileData& data)
{
    for (const auto& entry : m_oldKeyframes) {
        if (ObjectData* object = findObject(data, entry.first))
            object->keyframes = entry.second;
    }
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
        m_captured = true;
    }
    const int index = keyframeIndexAt(object->keyframes, m_oldT);
    if (index < 0)
        return;
    Keyframe moved = object->keyframes.at(index);
    moved.t = m_newT;
    object->keyframes.removeAt(index);
    upsertKeyframe(object->keyframes, moved); // 目标时刻已有帧 → 覆盖
    ensureDuration(data, m_newT);
}

void MoveKeyframeCommand::revert(FileData& data)
{
    if (ObjectData* object = findObject(data, m_id))
        object->keyframes = m_oldKeyframes;
    data.timeline.duration = m_oldDuration;
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
