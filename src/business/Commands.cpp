#include "Commands.h"

#include <QHash>
#include <QSet>

namespace {

ObjectData* findObject(FileData& data, const QString& id)
{
    for (ObjectData& object : data.objects) {
        if (object.id == id)
            return &object;
    }
    return nullptr;
}

// id → 直系父 id 映射
QHash<QString, QString> parentMap(const FileData& data)
{
    QHash<QString, QString> parents;
    for (const ObjectData& object : data.objects)
        parents.insert(object.id, object.parentId);
    return parents;
}

// descendant 是否为 ancestor 的后代（沿父链上溯；带环保护，畸形数据不死循环）
bool isDescendantOf(const QHash<QString, QString>& parents, const QString& id, const QString& ancestor)
{
    QString p = parents.value(id);
    for (int guard = 0; !p.isEmpty() && guard <= parents.size(); ++guard) {
        if (p == ancestor)
            return true;
        p = parents.value(p);
    }
    return false;
}

int indexOfId(const FileData& data, const QString& id)
{
    for (int i = 0; i < data.objects.size(); ++i) {
        if (data.objects.at(i).id == id)
            return i;
    }
    return -1;
}

} // namespace

// ---- SetTransformsCommand ----

SetTransformsCommand::SetTransformsCommand(QList<ObjectTransform> updates)
    : m_updates(std::move(updates))
{
}

void SetTransformsCommand::apply(FileData& data)
{
    if (m_previous.isEmpty()) {
        for (const ObjectTransform& update : m_updates) {
            const ObjectData* object = findObject(data, update.id);
            m_previous.append(object ? object->transform : update.value);
        }
    }
    for (const ObjectTransform& update : m_updates) {
        if (ObjectData* object = findObject(data, update.id))
            object->transform = update.value;
    }
}

void SetTransformsCommand::revert(FileData& data)
{
    for (int i = 0; i < m_updates.size(); ++i) {
        if (ObjectData* object = findObject(data, m_updates.at(i).id))
            object->transform = m_previous.at(i);
    }
}

// ---- AddObjectsCommand ----

AddObjectsCommand::AddObjectsCommand(QList<ObjectData> objects)
    : m_objects(std::move(objects))
{
}

void AddObjectsCommand::apply(FileData& data)
{
    for (const ObjectData& object : m_objects)
        data.objects.append(object);
}

void AddObjectsCommand::revert(FileData& data)
{
    for (const ObjectData& object : m_objects) {
        for (int i = data.objects.size() - 1; i >= 0; --i) {
            if (data.objects.at(i).id == object.id)
                data.objects.removeAt(i);
        }
    }
}

// ---- RemoveObjectsCommand ----

RemoveObjectsCommand::RemoveObjectsCommand(QList<QString> ids)
    : m_ids(std::move(ids))
{
}

void RemoveObjectsCommand::apply(FileData& data)
{
    if (!m_captured) {
        // 级联：被删集合 = 指定 id + 它们的所有后代
        const QHash<QString, QString> parents = parentMap(data);
        QSet<QString> cascade;
        for (const ObjectData& object : data.objects) {
            for (const QString& id : m_ids) {
                if (object.id == id || isDescendantOf(parents, object.id, id)) {
                    cascade.insert(object.id);
                    break;
                }
            }
        }
        for (int i = 0; i < data.objects.size(); ++i) {
            if (cascade.contains(data.objects.at(i).id))
                m_removed.append(qMakePair(i, data.objects.at(i)));
        }
        m_oldPaths = data.paths; // 阶段12：路径级联删除的快照（整条列表）
        m_captured = true;
    }
    QSet<QString> removedIds;
    for (const auto& entry : m_removed)
        removedIds.insert(entry.second.id);
    for (int i = data.objects.size() - 1; i >= 0; --i) {
        if (removedIds.contains(data.objects.at(i).id))
            data.objects.removeAt(i);
    }
    // 阶段12 级联：被删对象（含级联后代）的全部 PathData 一并删除（redo 重放幂等）
    for (int i = data.paths.size() - 1; i >= 0; --i) {
        if (removedIds.contains(data.paths.at(i).objectId))
            data.paths.removeAt(i);
    }
}

void RemoveObjectsCommand::revert(FileData& data)
{
    // m_removed 按下标升序记录，顺序插回即还原原始排列
    for (const QPair<int, ObjectData>& entry : m_removed)
        data.objects.insert(qMin(entry.first, data.objects.size()), entry.second);
    data.paths = m_oldPaths; // 阶段12：路径级联一并还原
}

// ---- ReparentObjectsCommand ----

ReparentObjectsCommand::ReparentObjectsCommand(QList<QString> ids, QString newParentId,
                                               int insertIndex)
    : m_ids(std::move(ids))
    , m_newParentId(std::move(newParentId))
    , m_insertIndex(insertIndex)
{
}

void ReparentObjectsCommand::apply(FileData& data)
{
    if (!m_captured) {
        m_previousObjects = data.objects; // 旧 parentId 与原位置的整体快照
        m_captured = true;
    }

    const QHash<QString, QString> parents = parentMap(data);
    const QSet<QString> idSet(m_ids.begin(), m_ids.end());

    // 移动根：被移动集合中、祖先不在集合内的对象（后代随根移动），按当前文件顺序
    QList<QString> roots;
    for (const ObjectData& object : data.objects) {
        if (!idSet.contains(object.id))
            continue;
        bool ancestorMoved = false;
        for (const QString& other : idSet) {
            if (other != object.id && isDescendantOf(parents, object.id, other)) {
                ancestorMoved = true;
                break;
            }
        }
        if (!ancestorMoved)
            roots.append(object.id);
    }

    // 逐棵提取子树块（DFS 不变式保证连续区间）并从列表移除；根行改挂新父
    QList<QList<ObjectData>> blocks;
    for (const QString& root : roots) {
        const int start = indexOfId(data, root); // 每次移除后下标前移，需重查
        if (start < 0)
            continue;
        int end = start + 1;
        while (end < data.objects.size()
               && isDescendantOf(parents, data.objects.at(end).id, root))
            ++end;
        QList<ObjectData> block;
        for (int i = start; i < end; ++i)
            block.append(data.objects.at(i));
        block.first().parentId = m_newParentId;
        blocks.append(block);
        for (int i = end - 1; i >= start; --i)
            data.objects.removeAt(i);
    }

    // 在剩余列表中定位插入点：newParent 的第 insertIndex 个直系子对象之前，
    // 越界则排到其父的子树末尾（顶层 = 列表末尾）
    QList<int> siblings; // 剩余列表中 newParentId 的直系子对象下标
    for (int i = 0; i < data.objects.size(); ++i) {
        if (data.objects.at(i).parentId == m_newParentId)
            siblings.append(i);
    }
    int pos = -1;
    if (m_insertIndex >= 0 && m_insertIndex < siblings.size()) {
        pos = siblings.at(m_insertIndex);
    } else if (m_newParentId.isEmpty()) {
        pos = data.objects.size();
    } else {
        const int parentIndex = indexOfId(data, m_newParentId);
        if (parentIndex < 0) {
            pos = data.objects.size(); // 兜底：父不存在则落顶层末尾
        } else {
            pos = parentIndex + 1;
            while (pos < data.objects.size()
                   && isDescendantOf(parents, data.objects.at(pos).id, m_newParentId))
                ++pos;
        }
    }

    for (const QList<ObjectData>& block : blocks) {
        for (const ObjectData& object : block)
            data.objects.insert(pos++, object);
    }
}

void ReparentObjectsCommand::revert(FileData& data)
{
    data.objects = m_previousObjects; // parentId 与排列逐字段还原
}

// ---- RenameObjectCommand ----

RenameObjectCommand::RenameObjectCommand(QString id, QString newName)
    : m_id(std::move(id))
    , m_newName(std::move(newName))
{
}

void RenameObjectCommand::apply(FileData& data)
{
    ObjectData* object = findObject(data, m_id);
    if (!object)
        return;
    if (!m_captured) {
        m_oldName = object->name;
        m_captured = true;
    }
    object->name = m_newName;
}

void RenameObjectCommand::revert(FileData& data)
{
    if (ObjectData* object = findObject(data, m_id))
        object->name = m_oldName;
}

// ---- SetObjectColorCommand ----

SetObjectColorCommand::SetObjectColorCommand(QString id, QColor newColor)
    : m_id(std::move(id))
    , m_newColor(std::move(newColor))
{
}

void SetObjectColorCommand::apply(FileData& data)
{
    ObjectData* object = findObject(data, m_id);
    if (!object)
        return;
    if (!m_captured) {
        m_oldColor = object->color;
        m_captured = true;
    }
    object->color = m_newColor;
}

void SetObjectColorCommand::revert(FileData& data)
{
    if (ObjectData* object = findObject(data, m_id))
        object->color = m_oldColor;
}

// ---- SetScreenSizeCommand ----

SetScreenSizeCommand::SetScreenSizeCommand(QString id, QSizeF newSize)
    : m_id(std::move(id))
    , m_newSize(std::move(newSize))
{
}

void SetScreenSizeCommand::apply(FileData& data)
{
    ObjectData* object = findObject(data, m_id);
    if (!object || !object->size)
        return;
    if (!m_captured) {
        m_oldSize = *object->size;
        m_captured = true;
    }
    object->size = m_newSize;
}

void SetScreenSizeCommand::revert(FileData& data)
{
    if (ObjectData* object = findObject(data, m_id))
        object->size = m_oldSize;
}
