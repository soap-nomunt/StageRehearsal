#include "EditorCore.h"

#include "KeyframeCommands.h"
#include "PathCommands.h"
#include "PathGeometry.h"
#include "ResourceCommands.h"
#include "data/AppSettings.h"
#include "data/ResourceFiles.h"
#include "data/RuntimeData.h"

#include <QHash>
#include <QUuid>

namespace {

QString idPrefixFor(ObjectType type)
{
    switch (type) {
    case ObjectType::Actor:  return QStringLiteral("actor");
    case ObjectType::Screen: return QStringLiteral("screen");
    case ObjectType::Prop:   return QStringLiteral("prop");
    case ObjectType::Group:  return QStringLiteral("group");
    }
    return QStringLiteral("actor");
}

} // namespace

EditorCore::EditorCore(RuntimeData* runtimeData, QObject* parent)
    : QObject(parent)
    , m_runtimeData(runtimeData)
    , m_timelinePlayer(runtimeData)
    , m_previewPlayer(runtimeData)
{
    // 数据替换后剔除已不存在对象的选中态（删除/撤销/打开新文件等）
    connect(m_runtimeData, &RuntimeData::fileDataChanged, this, &EditorCore::pruneSelection);
    // 阶段12：命令提交/撤销后防御性剔除失效的选中路径（段或路径点不存在即清除）
    connect(m_runtimeData, &RuntimeData::fileDataChanged, this, &EditorCore::pruneActivePath);
    // 阶段14：资源删除/撤销后剔除失效的资源选中态
    connect(m_runtimeData, &RuntimeData::fileDataChanged,
            this, &EditorCore::pruneActiveResource);
    // 阶段14：选中资源后若其相对路径被修改（撤销/重做等），当前试听停止并重新加载
    connect(m_runtimeData, &RuntimeData::fileDataChanged, this, [this] {
        if (m_activeResourceId.isEmpty())
            return;
        const ResourceData* resource = findResource(m_activeResourceId);
        if (!resource)
            return; // pruneActiveResource 负责清空
        if (!m_previewPlayer.resourceId().isEmpty()
            && m_previewPlayer.resourceId() == resource->id
            && m_previewPlayer.relativePath() != resource->relativePath) {
            m_previewPlayer.setResource(*resource);
        }
    });
    // 阶段12：对象选中变化清除选中路径（选中对象 → Inspector 回对象页；
    // 清空选择不动路径——路径点方向键微调依赖无对象选中 + activePath 共存）
    // 阶段14：对象选中同时清除资源选中态（Inspector 回对象页）
    connect(&m_selection, &SelectionModel::selectionChanged, this, [this] {
        if (!m_selection.isEmpty())
            setActiveResource(QString());
        if (m_activePath && !m_selection.isEmpty())
            setActivePath(std::nullopt);
    });
    // 文件整体替换（新建/打开成功）时清空撤销栈与资源选中态/试听
    connect(m_runtimeData, &RuntimeData::fileReplaced, this, [this] {
        m_undoStack.clear();
        emit undoStackChanged();
        m_activeResourceId.clear();
        emit activeResourceChanged();
        m_previewPlayer.clear();
    });
    // 阶段14：试听与时间轴播放互斥——开始试听暂停时间轴；时间轴开始播放
    // 停止试听，之后不自动恢复
    connect(&m_previewPlayer, &ResourcePreviewPlayer::playingChanged, this, [this](bool playing) {
        if (playing && m_timelinePlayer.isPlaying())
            m_timelinePlayer.pause();
    });
    connect(&m_timelinePlayer, &TimelinePlayer::playingChanged, this, [this](bool playing) {
        if (playing)
            m_previewPlayer.stop();
    });
}

bool EditorCore::executeCommand(std::unique_ptr<Command> command)
{
    if (!command)
        return false;
    FileData working = m_runtimeData->fileData();
    const FileData before = working;
    command->apply(working);
    if (working == before)
        return false; // 无实际变更：不提交、不入栈，避免无谓的场景重建
    m_runtimeData->commitFileData(working);
    m_undoStack.push(std::move(command)); // 入栈点唯一：提交成功才入栈
    emit undoStackChanged();
    return true;
}

bool EditorCore::undo()
{
    FileData working = m_runtimeData->fileData();
    if (!m_undoStack.undo(working))
        return false;
    m_runtimeData->commitFileData(working);
    emit undoStackChanged();
    return true;
}

bool EditorCore::redo()
{
    FileData working = m_runtimeData->fileData();
    if (!m_undoStack.redo(working))
        return false;
    m_runtimeData->commitFileData(working);
    emit undoStackChanged();
    return true;
}

const ObjectData* EditorCore::findObject(const QString& id) const
{
    for (const ObjectData& object : m_runtimeData->fileData().objects) {
        if (object.id == id)
            return &object;
    }
    return nullptr;
}

bool EditorCore::isNameTaken(const QString& name, const QString& excludeId) const
{
    for (const ObjectData& object : m_runtimeData->fileData().objects) {
        if (object.id != excludeId && object.name == name)
            return true;
    }
    return false;
}

const ResourceData* EditorCore::findResource(const QString& id) const
{
    for (const ResourceData& resource : m_runtimeData->fileData().resources) {
        if (resource.id == id)
            return &resource;
    }
    return nullptr;
}

bool EditorCore::isResourceNameTaken(const QString& name, const QString& excludeId) const
{
    for (const ResourceData& resource : m_runtimeData->fileData().resources) {
        if (resource.id != excludeId && resource.name == name)
            return true;
    }
    return false;
}

// ---- 创建 / 剪贴板 ----

bool EditorCore::createObject(ObjectType type, const QPointF& posCm)
{
    ObjectData object;
    object.id = generateObjectId(type);
    object.type = type;
    object.name = generateObjectName(type);
    if (type == ObjectType::Group) {
        // 空对象：纯组织节点，任何入口都落顶层（parentId 默认空串）、忽略位置，
        // 无颜色/大小属性，transform/keyframes 保留默认值
    } else {
        object.color = nextPaletteColor();
        object.transform = { posCm.x(), posCm.y(), 0.0 };
        if (type == ObjectType::Screen) {
            object.size =
                QSizeF(AppSettings::screenDefaultWidthCm, AppSettings::screenDefaultHeightCm);
        }
        // 道具（阶段13）：shape 缺省 = 默认 100×100cm 矩形、scale=1、standable=true，
        // 取成员默认值即可，无需显式赋值
    }

    const QString newId = object.id;
    if (!executeCommand(std::make_unique<AddObjectsCommand>(QList<ObjectData> { object })))
        return false;
    m_selection.select(newId); // 创建后自动选中
    return true;
}

void EditorCore::copySelection()
{
    const QList<QString>& ids = m_selection.selectedIds();
    if (ids.isEmpty())
        return;
    m_clipboard.clear();
    // 连同整棵子树一起复制：被删集合 = 选中 id + 所有后代，按文件顺序深拷贝
    const QList<ObjectData>& objects = m_runtimeData->fileData().objects;
    for (const ObjectData& object : objects) {
        bool included = ids.contains(object.id);
        for (QString p = object.parentId; !included && !p.isEmpty();) {
            if (ids.contains(p)) {
                included = true;
                break;
            }
            const ObjectData* parent = findObject(p);
            p = parent ? parent->parentId : QString();
        }
        if (included)
            m_clipboard.append(object);
    }
    emit clipboardChanged();
}

bool EditorCore::pasteClipboard(const std::optional<QPointF>& atCm)
{
    if (m_clipboard.isEmpty())
        return false;

    QSet<QString> taken;
    QSet<QString> takenNames;
    for (const ObjectData& object : m_runtimeData->fileData().objects) {
        taken.insert(object.id);
        takenNames.insert(object.name);
    }

    QSet<QString> clipboardIds;
    for (const ObjectData& object : m_clipboard)
        clipboardIds.insert(object.id);

    // 整棵子树一起粘贴：旧 id → 新 id 映射，保持子树结构；根（父不在剪贴板内）落顶层
    QHash<QString, QString> idMap;
    for (const ObjectData& source : m_clipboard)
        idMap.insert(source.id, nextFreeId(source.type, taken));

    // 锚点：剪贴板中第一个有位置的根对象（空对象无位置概念，不参与偏移）
    const auto isRoot = [&clipboardIds](const ObjectData& object) {
        return !clipboardIds.contains(object.parentId);
    };
    QPointF anchor;
    bool anchorSet = false;
    for (const ObjectData& source : m_clipboard) {
        if (isRoot(source) && source.type != ObjectType::Group) {
            anchor = QPointF(source.transform.x, source.transform.y);
            anchorSet = true;
            break;
        }
    }

    // m_clipboard 按文件顺序（DFS），粘贴块内部保持该顺序即满足层级不变式
    QList<ObjectData> objects;
    QList<QString> newIds;
    for (const ObjectData& source : m_clipboard) {
        ObjectData copy = source; // 深拷贝（值类型，修改即分离）
        copy.id = idMap.value(source.id);
        copy.name = uniqueName(copy.name, takenNames);
        if (isRoot(source)) {
            copy.parentId.clear(); // 粘贴到顶层
            // 偏移只作用于有位置的根对象；子对象保持绝对场景坐标，不随父移动
            if (source.type != ObjectType::Group) {
                QPointF delta(20.0, 20.0); // 默认 / Hierarchy 粘贴：原坐标偏移 (x+20, y+20)
                if (atCm && anchorSet)
                    delta = QPointF(atCm->x(), atCm->y()) - anchor; // 右键粘贴：锚点落鼠标处
                copy.transform.x = source.transform.x + delta.x();
                copy.transform.y = source.transform.y + delta.y();
                // 关键帧位置随副本整体偏移（时刻与朝向不变），副本走位与原件一致地平移
                for (Keyframe& key : copy.keyframes) {
                    key.x += delta.x();
                    key.y += delta.y();
                }
            }
        } else {
            copy.parentId = idMap.value(source.parentId); // 保持子树结构
        }
        objects.append(copy);
        newIds.append(copy.id);
    }

    if (!executeCommand(std::make_unique<AddObjectsCommand>(objects)))
        return false;
    m_selection.setSelection(newIds);
    return true;
}

bool EditorCore::deleteSelection()
{
    const QList<QString>& ids = m_selection.selectedIds();
    if (ids.isEmpty())
        return false;
    // 级联删除整棵子树由 RemoveObjectsCommand 完成；
    // 删除后 pruneSelection 随 fileDataChanged 自动清空对应选中态
    return executeCommand(std::make_unique<RemoveObjectsCommand>(ids));
}

// ---- 层级整理 ----

bool EditorCore::reparent(const QList<QString>& ids, const QString& newParentId, int insertIndex)
{
    if (ids.isEmpty())
        return false;
    const FileData& data = m_runtimeData->fileData();

    if (!newParentId.isEmpty()) {
        const ObjectData* newParent = findObject(newParentId);
        if (!newParent || newParent->type == ObjectType::Prop)
            return false; // 父必须存在；Prop 不出现
    }

    QSet<QString> idSet;
    for (const QString& id : ids) {
        const ObjectData* object = findObject(id);
        if (!object)
            return false;
        idSet.insert(id);
    }

    // 禁止环：新父不能是被移动对象本身或其后代（沿父链上溯遇到移动集合即拒绝）
    for (QString p = newParentId; !p.isEmpty();) {
        if (idSet.contains(p))
            return false;
        const ObjectData* parent = findObject(p);
        p = parent ? parent->parentId : QString();
    }

    return executeCommand(
        std::make_unique<ReparentObjectsCommand>(ids, newParentId, insertIndex));
}

// ---- 变换 / 属性（游标时刻语义，需求第 20 条）----

bool EditorCore::setTransformsAtCursor(const QList<ObjectTransform>& updates,
                                       const KeyframeConfirmCallback& confirm)
{
    const double t = m_timelinePlayer.currentTime();
    // 空对象无关键帧语义：选区里混入的分组一律跳过
    QList<ObjectKeyframe> batch;
    bool anyMissingAtCursor = false;
    for (const ObjectTransform& update : updates) {
        const ObjectData* object = findObject(update.id);
        if (!object || object->type == ObjectType::Group)
            continue;
        // 值等于当前显示值（无帧时 = 静态/插值值）→ 无视觉变化，跳过
        // （零位移拖拽不产生关键帧也不弹窗；显式打帧走 keyframeSelectionAtCursor）
        const Transform shown = TimelinePlayer::interpolate(
            *object, m_runtimeData->fileData().paths, t);
        if (update.value == shown)
            continue;
        bool hasAtCursor = false;
        for (const Keyframe& existing : object->keyframes) {
            if (qAbs(existing.t - t) <= TimelinePlayer::kTimeEpsilon) {
                hasAtCursor = true;
                break;
            }
        }
        anyMissingAtCursor = anyMissingAtCursor || !hasAtCursor;
        batch.append({ update.id,
                       { t, update.value.x, update.value.y, update.value.rotation } });
    }
    if (batch.isEmpty())
        return false;
    // 全部对象在 T 处已有帧 → 直接更新不询问；否则按设置/回调询问一次（整批一个决定）
    if (anyMissingAtCursor && !m_runtimeData->settings().keyframePromptSuppressed()
        && (!confirm || !confirm()))
        return false; // 选"否"：零变更，渲染层把图元回弹到插值显示位置
    return executeCommand(std::make_unique<UpsertKeyframesCommand>(batch));
}

bool EditorCore::moveSelectionBy(const QPointF& deltaCm,
                                 const KeyframeConfirmCallback& confirm)
{
    QList<ObjectTransform> updates;
    for (const QString& id : m_selection.selectedIds()) {
        const ObjectData* object = findObject(id);
        if (!object || object->type == ObjectType::Group)
            continue;
        // 以当前时刻显示值（插值结果）为基准平移，提交新绝对值
        const Transform base = TimelinePlayer::interpolate(
            *object, m_runtimeData->fileData().paths, m_timelinePlayer.currentTime());
        updates.append({ id,
                         { base.x + deltaCm.x(), base.y + deltaCm.y(), base.rotation } });
    }
    return setTransformsAtCursor(updates, confirm);
}

bool EditorCore::wouldTransformBlocked(const QList<ObjectTransform>& updates) const
{
    // 与 setTransformsAtCursor 同一过滤（空对象/不存在/值等于当前显示值跳过）；
    // 模拟应用（含级联）后判定——纯旋转/零位移条目不触发校验
    const double t = m_timelinePlayer.currentTime();
    QList<ObjectKeyframe> items;
    for (const ObjectTransform& update : updates) {
        const ObjectData* object = findObject(update.id);
        if (!object || object->type == ObjectType::Group)
            continue;
        const Transform shown = TimelinePlayer::interpolate(
            *object, m_runtimeData->fileData().paths, t);
        if (update.value == shown)
            continue;
        items.append({ update.id,
                       { t, update.value.x, update.value.y, update.value.rotation } });
    }
    if (items.isEmpty())
        return false;
    return keyframeEditsBlocked(m_runtimeData->fileData(), items);
}

bool EditorCore::canKeyframeSelection() const
{
    const QString id = m_selection.singleSelectedId();
    if (id.isEmpty())
        return false;
    const ObjectData* object = findObject(id);
    return object && (object->type == ObjectType::Actor || object->type == ObjectType::Screen
                      || object->type == ObjectType::Prop); // 阶段13：道具可打帧
}

bool EditorCore::keyframeSelectionAtCursor()
{
    if (!canKeyframeSelection())
        return false;
    const QString id = m_selection.singleSelectedId();
    Transform value;
    if (!displayTransform(id, value))
        return false;
    const double t = m_timelinePlayer.currentTime();
    return executeCommand(std::make_unique<UpsertKeyframesCommand>(
        QList<ObjectKeyframe> { { id, { t, value.x, value.y, value.rotation } } }));
}

bool EditorCore::displayTransform(const QString& id, Transform& out) const
{
    const ObjectData* object = findObject(id);
    if (!object)
        return false;
    out = TimelinePlayer::interpolate(*object, m_runtimeData->fileData().paths,
                                      m_timelinePlayer.currentTime());
    return true;
}

// ---- 关键帧 / 场次标签编辑 ----

bool EditorCore::upsertKeyframe(const QString& id, const Keyframe& keyframe)
{
    const ObjectData* object = findObject(id);
    if (!object || object->type == ObjectType::Group)
        return false; // 空对象无关键帧语义
    return executeCommand(std::make_unique<UpsertKeyframesCommand>(
        QList<ObjectKeyframe> { { id, keyframe } }));
}

bool EditorCore::removeKeyframes(const QList<QPair<QString, double>>& refs)
{
    if (refs.isEmpty())
        return false;
    QList<ObjectKeyframeRef> list;
    for (const QPair<QString, double>& ref : refs)
        list.append({ ref.first, ref.second });
    return executeCommand(std::make_unique<RemoveKeyframesCommand>(list));
}

bool EditorCore::moveKeyframe(const QString& id, double oldT, double newT)
{
    if (!findObject(id))
        return false;
    if (qAbs(oldT - newT) <= TimelinePlayer::kTimeEpsilon)
        return false; // 时刻未变：无变更不入栈
    return executeCommand(std::make_unique<MoveKeyframeCommand>(id, oldT, newT));
}

bool EditorCore::moveKeyframes(const QList<QPair<QString, double>>& refs, double deltaT)
{
    if (refs.isEmpty() || qAbs(deltaT) <= TimelinePlayer::kTimeEpsilon)
        return false;
    QList<ObjectKeyframeRef> list;
    for (const QPair<QString, double>& ref : refs)
        list.append({ ref.first, ref.second });
    return executeCommand(std::make_unique<MoveKeyframesCommand>(list, deltaT));
}

bool EditorCore::moveSceneLabel(int index, double newT)
{
    const QList<SceneLabel>& labels = m_runtimeData->fileData().timeline.labels;
    if (index < 0 || index >= labels.size())
        return false;
    const double clamped = qMax(0.0, newT);
    if (qAbs(labels.at(index).t - clamped) <= TimelinePlayer::kTimeEpsilon)
        return false; // 无变更不入栈
    return executeCommand(std::make_unique<MoveSceneLabelCommand>(index, clamped));
}

// ---- 关键帧级剪贴板 ----

void EditorCore::copySelectedKeyframes(const QList<QPair<QString, double>>& refs)
{
    m_keyframeClipboard.clear();
    for (const QPair<QString, double>& ref : refs) {
        const ObjectData* object = findObject(ref.first);
        if (!object || object->type == ObjectType::Group)
            continue;
        for (const Keyframe& key : object->keyframes) {
            if (qAbs(key.t - ref.second) <= TimelinePlayer::kTimeEpsilon) {
                m_keyframeClipboard.append({ ref.first, key }); // 值类型深拷贝
                break;
            }
        }
    }
}

bool EditorCore::pasteKeyframesAtCursor()
{
    if (m_keyframeClipboard.isEmpty())
        return false;
    // 复制集最小 t 对齐游标，其余帧保持相对间隔整体平移
    double minT = m_keyframeClipboard.first().keyframe.t;
    for (const ObjectKeyframe& entry : m_keyframeClipboard)
        minT = qMin(minT, entry.keyframe.t);
    const double delta = m_timelinePlayer.currentTime() - minT;
    QList<ObjectKeyframe> batch;
    for (const ObjectKeyframe& entry : m_keyframeClipboard) {
        const ObjectData* object = findObject(entry.id);
        if (!object || object->type == ObjectType::Group)
            continue; // 目标对象已删除：跳过
        Keyframe key = entry.keyframe;
        key.t += delta;
        batch.append({ entry.id, key });
    }
    if (batch.isEmpty())
        return false;
    return executeCommand(std::make_unique<UpsertKeyframesCommand>(batch));
}

bool EditorCore::addSceneLabel(double t, const QString& name)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return false;
    return executeCommand(
        std::make_unique<AddSceneLabelCommand>(SceneLabel { t, trimmed }));
}

bool EditorCore::renameSceneLabel(int index, const QString& name)
{
    const QList<SceneLabel>& labels = m_runtimeData->fileData().timeline.labels;
    const QString trimmed = name.trimmed();
    if (index < 0 || index >= labels.size() || trimmed.isEmpty())
        return false;
    if (labels.at(index).name == trimmed)
        return false; // 无变更
    return executeCommand(std::make_unique<RenameSceneLabelCommand>(index, trimmed));
}

bool EditorCore::removeSceneLabel(int index)
{
    const QList<SceneLabel>& labels = m_runtimeData->fileData().timeline.labels;
    if (index < 0 || index >= labels.size())
        return false;
    return executeCommand(std::make_unique<RemoveSceneLabelCommand>(index));
}

bool EditorCore::setObjectName(const QString& id, const QString& name)
{
    const ObjectData* object = findObject(id);
    const QString trimmed = name.trimmed();
    if (!object || trimmed.isEmpty() || trimmed == object->name)
        return false;
    if (isNameTaken(trimmed, id))
        return false;
    return executeCommand(std::make_unique<RenameObjectCommand>(id, trimmed));
}

bool EditorCore::setObjectColor(const QString& id, const QColor& color)
{
    const ObjectData* object = findObject(id);
    if (!object || !color.isValid() || object->color == color)
        return false;
    return executeCommand(std::make_unique<SetObjectColorCommand>(id, color));
}

bool EditorCore::setScreenSize(const QString& id, const QSizeF& sizeCm)
{
    const ObjectData* object = findObject(id);
    if (!object || !object->size || object->size.value() == sizeCm)
        return false;
    if (sizeCm.width() <= 0.0 || sizeCm.height() <= 0.0)
        return false;
    return executeCommand(std::make_unique<SetScreenSizeCommand>(id, sizeCm));
}

// ---- 道具属性（阶段13）----

bool EditorCore::setPropScale(const QString& id, double scaleX, double scaleY)
{
    const ObjectData* object = findObject(id);
    if (!object || object->type != ObjectType::Prop)
        return false;
    if (scaleX < 0.1 || scaleX > 100.0 || scaleY < 0.1 || scaleY > 100.0)
        return false; // 范围 0.1~100 仅正值（防御；Inspector spin 已钳制）
    return executeCommand(std::make_unique<SetPropScaleCommand>(id, scaleX, scaleY));
}

bool EditorCore::setPropStandable(const QString& id, bool standable)
{
    const ObjectData* object = findObject(id);
    if (!object || object->type != ObjectType::Prop)
        return false;
    return executeCommand(std::make_unique<SetPropStandableCommand>(id, standable));
}

bool EditorCore::setPropShape(const QString& id, const PropShape& shape)
{
    const ObjectData* object = findObject(id);
    if (!object || object->type != ObjectType::Prop)
        return false;
    return executeCommand(std::make_unique<SetPropShapeCommand>(id, shape));
}

bool EditorCore::setStage(const StageData& stage)
{
    if (stage.width <= 0.0 || stage.depth <= 0.0)
        return false; // 非法尺寸拒绝（centerOffset 任意 double）
    FileData working = m_runtimeData->fileData();
    if (working.stage == stage)
        return false; // 无变更不提交
    // 直改直提交：舞台不入撤销栈（撤销范围 = 主视口/Hierarchy/Inspector/时间轴
    // 的可操作对象，舞台只能在设置窗口调整）；提交仍走 commitFileData，
    // dirty 跟踪与 fileDataChanged 刷新路径与命令提交一致
    working.stage = stage;
    m_runtimeData->commitFileData(working);
    return true;
}

// ---- 双帧段选中态（阶段12）----

void EditorCore::setActiveSegment(const std::optional<SegmentRef>& segment)
{
    if (m_activeSegment == segment)
        return; // 相同值不重发信号（渲染层每帧都可能重报）
    m_activeSegment = segment;
    emit activeSegmentChanged();
    // 阶段14：选中双帧段（时间轴帧选择）时清除资源选中态（不重复触发
    // setActiveResource，避免其反过来清掉刚设置的段）
    if (segment && !m_activeResourceId.isEmpty()) {
        m_activeResourceId.clear();
        emit activeResourceChanged();
        m_previewPlayer.clear();
    }
    // 双帧态消失/变更：选中路径一并清除（白线只在双帧态显示）
    if (m_activePath
        && (!segment || m_activePath->segment != *segment))
        setActivePath(std::nullopt);
}

void EditorCore::setActivePath(const std::optional<ActivePathRef>& path)
{
    if (m_activePath == path)
        return;
    m_activePath = path;
    emit activePathChanged();
    // 阶段14：选中路径时清除资源选中态（Inspector 回路径页）
    if (path && !m_activeResourceId.isEmpty()) {
        m_activeResourceId.clear();
        emit activeResourceChanged();
        m_previewPlayer.clear();
    }
}

void EditorCore::pruneActivePath()
{
    if (!m_activePath)
        return;
    const ActivePathRef& path = *m_activePath;
    // 段帧存在性（选中路径的段必须仍是该对象的相邻两帧标识——帧存在即可，
    // 相邻性/双帧态由画布侧维护）
    QPointF fromPos;
    QPointF toPos;
    if (!resolveSegmentEndpoints(m_runtimeData->fileData(), path.segment.objectId,
                                 path.segment.from, path.segment.to, fromPos, toPos)) {
        setActivePath(std::nullopt);
        return;
    }
    // 路径点下标越界（删点/撤销后）→ 回退为仅路径选中
    if (path.pointIndex >= 0) {
        const PathData* data = findPath(path.segment.objectId, path.segment.from,
                                        path.segment.to);
        const int count = data ? data->points.size() : 0;
        if (path.pointIndex >= count) {
            ActivePathRef fixed = path;
            fixed.pointIndex = count - 1; // 删空 → -1（仅路径选中）
            setActivePath(fixed);
        }
    }
}

// ---- 项目资源库（阶段14）----

QString EditorCore::generateResourceId() const
{
    QSet<QString> taken;
    for (const ResourceData& resource : m_runtimeData->fileData().resources)
        taken.insert(resource.id);
    QString id;
    do {
        id = QStringLiteral("res-")
            + QUuid::createUuid().toString(QUuid::WithoutBraces);
    } while (taken.contains(id));
    return id;
}

QString EditorCore::uniqueResourceName(const QString& baseName,
                                       const QSet<QString>& reservedNames) const
{
    QString base = baseName.trimmed();
    if (base.isEmpty())
        base = QStringLiteral("资源");
    const auto taken = [this, &reservedNames](const QString& name) {
        return reservedNames.contains(name) || isResourceNameTaken(name);
    };
    if (!taken(base))
        return base;
    for (int n = 2;; ++n) {
        const QString candidate = QStringLiteral("%1 (%2)").arg(base).arg(n);
        if (!taken(candidate))
            return candidate;
    }
}

bool EditorCore::addResources(const QList<ResourceData>& resources)
{
    if (resources.isEmpty())
        return false;

    QSet<QString> ids;
    QSet<QString> names;
    for (const ResourceData& resource : m_runtimeData->fileData().resources) {
        ids.insert(resource.id);
        names.insert(resource.name);
    }
    for (const ResourceData& resource : resources) {
        if (resource.id.isEmpty() || resource.name.trimmed().isEmpty()
            || resource.originalName.trimmed().isEmpty())
            return false;
        if (resource.originalName.contains(QLatin1Char('/'))
            || resource.originalName.contains(QLatin1Char('\\')))
            return false;
        if (!ResourceFiles::isSafeRelativePath(resource.relativePath))
            return false;
        if (ids.contains(resource.id) || names.contains(resource.name))
            return false; // id/别名全库唯一（批量内也查重）
        ids.insert(resource.id);
        names.insert(resource.name);
    }
    return executeCommand(std::make_unique<AddResourcesCommand>(resources));
}

bool EditorCore::removeResource(const QString& id)
{
    if (!findResource(id))
        return false;
    return executeCommand(std::make_unique<RemoveResourceCommand>(id));
}

bool EditorCore::renameResource(const QString& id, const QString& name)
{
    const ResourceData* resource = findResource(id);
    const QString trimmed = name.trimmed();
    if (!resource || trimmed.isEmpty() || trimmed == resource->name)
        return false;
    if (isResourceNameTaken(trimmed, id))
        return false;
    return executeCommand(std::make_unique<RenameResourceCommand>(id, trimmed));
}

void EditorCore::setActiveResource(const QString& id)
{
    if (id.isEmpty()) {
        if (m_activeResourceId.isEmpty())
            return;
        m_activeResourceId.clear();
        emit activeResourceChanged();
        m_previewPlayer.clear();
        return;
    }

    const ResourceData* resource = findResource(id);
    if (!resource) {
        setActiveResource(QString());
        return;
    }
    const ResourceType type = resource->type;
    if (m_activeResourceId != id) {
        m_activeResourceId = id;
        emit activeResourceChanged();
    }
    // 选择仲裁：资源选中清除舞台对象选择、双帧段和路径选择
    m_selection.clear();
    setActiveSegment(std::nullopt);
    setActivePath(std::nullopt);
    // 图片资源不保留音频试听；音频由 Inspector 资源页加载元数据
    if (type != ResourceType::Audio)
        m_previewPlayer.clear();
}

void EditorCore::pruneActiveResource()
{
    if (m_activeResourceId.isEmpty())
        return;
    if (!findResource(m_activeResourceId))
        setActiveResource(QString());
}

// ---- 移动路径（阶段12，全部走命令入撤销栈）----

const PathData* EditorCore::findPath(const QString& objectId, double from, double to) const
{
    return PathGeometry::findPathData(m_runtimeData->fileData().paths, objectId, from, to);
}

bool EditorCore::setPathType(const QString& objectId, double from, double to,
                             PathType type)
{
    if (!findObject(objectId))
        return false;
    return executeCommand(
        std::make_unique<SetPathTypeCommand>(objectId, from, to, type));
}

bool EditorCore::setPathLegAngle(const QString& objectId, double from, double to,
                                 int legIndex, double angleDeg)
{
    if (!findObject(objectId))
        return false;
    return executeCommand(
        std::make_unique<SetPathLegAngleCommand>(objectId, from, to, legIndex, angleDeg));
}

bool EditorCore::addPathPoint(const QString& objectId, double from, double to,
                              const QPointF& scenePos)
{
    if (!findObject(objectId))
        return false;
    return executeCommand(
        std::make_unique<AddPathPointCommand>(objectId, from, to, scenePos));
}

bool EditorCore::movePathPoint(const QString& objectId, double from, double to,
                               int pointIndex, const QPointF& scenePos)
{
    if (!findObject(objectId))
        return false;
    return executeCommand(
        std::make_unique<MovePathPointCommand>(objectId, from, to, pointIndex, scenePos));
}

bool EditorCore::removePathPoint(const QString& objectId, double from, double to,
                                 int pointIndex)
{
    if (!findObject(objectId))
        return false;
    return executeCommand(
        std::make_unique<RemovePathPointCommand>(objectId, from, to, pointIndex));
}

bool EditorCore::setPathPointT(const QString& objectId, double from, double to,
                               int pointIndex, double t)
{
    if (!findObject(objectId))
        return false;
    return executeCommand(
        std::make_unique<SetPathPointTCommand>(objectId, from, to, pointIndex, t));
}

bool EditorCore::resetPath(const QString& objectId, double from, double to)
{
    if (!findObject(objectId))
        return false;
    return executeCommand(std::make_unique<ResetPathCommand>(objectId, from, to));
}

// ---- 台下准备区（阶段10）----

const PrepAreaData* EditorCore::findPrepArea(const QString& id) const
{
    for (const PrepAreaData& area : m_runtimeData->fileData().prepAreas) {
        if (area.id == id)
            return &area;
    }
    return nullptr;
}

bool EditorCore::setPrepAreas(const QList<PrepAreaData>& prepAreas)
{
    QSet<QString> ids;
    QSet<QString> names;
    for (const PrepAreaData& area : prepAreas) {
        if (area.id.isEmpty() || area.name.isEmpty())
            return false; // id/name 必须非空
        if (area.width <= 0.0 || area.height <= 0.0)
            return false; // 非法尺寸拒绝
        if (ids.contains(area.id) || names.contains(area.name))
            return false; // id/name 全局唯一
        ids.insert(area.id);
        names.insert(area.name);
    }
    FileData working = m_runtimeData->fileData();
    if (working.prepAreas == prepAreas)
        return false; // 无变更不提交
    // 与 setStage 同例：只能在设置窗口调整，不入撤销栈；直改直提交，
    // dirty 跟踪与 fileDataChanged 刷新路径与命令提交一致
    working.prepAreas = prepAreas;
    m_runtimeData->commitFileData(working);
    return true;
}

QString EditorCore::addPrepArea(double x, double y)
{
    const QList<PrepAreaData>& existing = m_runtimeData->fileData().prepAreas;

    // 唯一 id：prep-NNN 递增查空（与对象 id 同惯例）
    QSet<QString> takenIds;
    for (const PrepAreaData& area : existing)
        takenIds.insert(area.id);
    int maxNumber = 0;
    for (const QString& id : takenIds) {
        if (id.startsWith(QLatin1String("prep-"))) {
            bool ok = false;
            const int number = id.mid(5).toInt(&ok);
            if (ok)
                maxNumber = qMax(maxNumber, number);
        }
    }
    QString newId;
    do {
        newId = QStringLiteral("prep-%1").arg(++maxNumber, 3, 10, QLatin1Char('0'));
    } while (takenIds.contains(newId));

    // 唯一名：准备区N，按现存最大 N+1（与 id 编号各自独立递增）
    int maxNameNumber = 0;
    const QString stem = QStringLiteral("准备区");
    for (const PrepAreaData& area : existing) {
        if (area.name.startsWith(stem)) {
            bool ok = false;
            const int number = area.name.mid(stem.size()).toInt(&ok);
            if (ok)
                maxNameNumber = qMax(maxNameNumber, number);
        }
    }
    QSet<QString> takenNames;
    for (const PrepAreaData& area : existing)
        takenNames.insert(area.name);
    QString newName;
    do {
        newName = stem + QString::number(++maxNameNumber);
    } while (takenNames.contains(newName));

    PrepAreaData area;
    area.id = newId;
    area.name = newName;
    area.x = x;
    area.y = y; // 默认 400×300 取 PrepAreaData 成员默认值
    QList<PrepAreaData> next = existing;
    next.append(area);
    if (!setPrepAreas(next))
        return QString();
    return newId;
}

bool EditorCore::removePrepArea(const QString& id)
{
    QList<PrepAreaData> next = m_runtimeData->fileData().prepAreas;
    for (qsizetype i = 0; i < next.size(); ++i) {
        if (next.at(i).id == id) {
            next.removeAt(i);
            return setPrepAreas(next);
        }
    }
    return false; // 不存在
}

// ---- 内部工具 ----

QString EditorCore::nextFreeId(ObjectType type, QSet<QString>& taken) const
{
    const QString prefix = idPrefixFor(type) + QLatin1Char('-');
    int maxNumber = 0;
    for (const QString& id : taken) {
        if (id.startsWith(prefix)) {
            bool ok = false;
            const int number = id.mid(prefix.size()).toInt(&ok);
            if (ok)
                maxNumber = qMax(maxNumber, number);
        }
    }
    QString candidate;
    do {
        candidate = prefix + QString::number(++maxNumber).rightJustified(3, QLatin1Char('0'));
    } while (taken.contains(candidate));
    taken.insert(candidate);
    return candidate;
}

QString EditorCore::generateObjectId(ObjectType type) const
{
    QSet<QString> taken;
    for (const ObjectData& object : m_runtimeData->fileData().objects)
        taken.insert(object.id);
    return nextFreeId(type, taken);
}

QString EditorCore::generateObjectName(ObjectType type) const
{
    QString stem;
    switch (type) {
    case ObjectType::Actor:  stem = QStringLiteral("角色");   break;
    case ObjectType::Screen: stem = QStringLiteral("屏风");   break;
    case ObjectType::Group:  stem = QStringLiteral("空对象"); break;
    case ObjectType::Prop:   stem = QStringLiteral("道具");   break;
    }
    for (int n = 1;; ++n) {
        const QString candidate = stem + QString::number(n);
        if (!isNameTaken(candidate))
            return candidate;
    }
}

QString EditorCore::uniqueName(const QString& base, QSet<QString>& takenNames) const
{
    const auto usable = [this, &takenNames](const QString& name) {
        return !takenNames.contains(name) && !isNameTaken(name);
    };
    if (usable(base)) {
        takenNames.insert(base);
        return base;
    }
    QString stem = base;
    while (!stem.isEmpty() && stem.back().isDigit())
        stem.chop(1);
    if (stem.isEmpty())
        stem = base;
    for (int n = 2;; ++n) {
        const QString candidate = stem + QString::number(n);
        if (usable(candidate)) {
            takenNames.insert(candidate);
            return candidate;
        }
    }
}

QColor EditorCore::nextPaletteColor()
{
    const QList<QColor> palette = AppSettings::defaultPalette();
    return palette.at((m_paletteCursor++) % palette.size());
}

void EditorCore::pruneSelection()
{
    QSet<QString> existing;
    for (const ObjectData& object : m_runtimeData->fileData().objects)
        existing.insert(object.id);
    QList<QString> kept;
    for (const QString& id : m_selection.selectedIds()) {
        if (existing.contains(id))
            kept.append(id);
    }
    if (kept.size() != m_selection.count())
        m_selection.setSelection(kept);
}
