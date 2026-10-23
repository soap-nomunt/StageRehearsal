#include "HierarchyPanel.h"

#include "business/EditorCore.h"
#include "business/SelectionModel.h"
#include "data/RuntimeData.h"
#include "dialogs/CreateObjectDialog.h"

#include <QApplication>
#include <QDropEvent>
#include <QHash>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPixmap>
#include <QSet>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <functional>

namespace {

// 树控件：点对象即选中（press 处理）；行上 press+move → 手动启动拖拽整理；
// 空白处交给基类：点击清空选择、拖动出橡皮筋框选（框选只从空白发起）。
// drop 不交给 Qt 改模型，只取落点信息经回调交给 HierarchyPanel 走命令。
class HierarchyTreeWidget : public QTreeWidget {
public:
    using QTreeWidget::QTreeWidget;

    // drop 落点回调：(目标行 id，OnViewport 为空串, DropZone)
    std::function<void(QString, int)> dropCallback;

    // 对外暴露落点枚举（QAbstractItemView::DropIndicatorPosition 是 protected，
    // 面板侧无法直接引用，这里按同值转出一版）
    enum DropZone {
        DropOnItem = QAbstractItemView::OnItem,
        DropAboveItem = QAbstractItemView::AboveItem,
        DropBelowItem = QAbstractItemView::BelowItem,
        DropOnViewport = QAbstractItemView::OnViewport
    };

protected:
    void mousePressEvent(QMouseEvent* event) override
    {
        m_pressItem = nullptr;
        if (event->button() == Qt::LeftButton) {
            const QPoint pos = event->position().toPoint();
            if (QTreeWidgetItem* item = itemAt(pos)) {
                if (isOnBranchIndicator(item, pos)) {
                    QTreeWidget::mousePressEvent(event); // 展开/折叠箭头交给基类
                    return;
                }
                m_pressItem = item;
                m_pressPos = pos;
                if (event->modifiers().testFlag(Qt::ControlModifier)) {
                    item->setSelected(!item->isSelected()); // Ctrl 切换
                } else if (!item->isSelected()) {
                    clearSelection();
                    item->setSelected(true); // 点未选中对象：立即单选
                }
                return; // 不进基类：避免从行上发起框选；拖拽由 mouseMove 手动启动
            }
        }
        QTreeWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        if (m_pressItem) {
            if (m_pressItem->isSelected()
                && (event->position().toPoint() - m_pressPos).manhattanLength()
                       >= QApplication::startDragDistance()) {
                // 行上长按 → 拖拽整理（当前选中整体移动）。
                // drop 可能触发树重建，此后不能再持有行指针
                m_pressItem = nullptr;
                startDrag(Qt::MoveAction);
            }
            return;
        }
        QTreeWidget::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (m_pressItem) {
            // 无 Ctrl 释放在已选中行上：收敛为单选（发生了拖拽则 m_pressItem 已清空）
            if (!event->modifiers().testFlag(Qt::ControlModifier) && m_pressItem->isSelected()) {
                clearSelection();
                m_pressItem->setSelected(true);
            }
            m_pressItem = nullptr;
            return;
        }
        QTreeWidget::mouseReleaseEvent(event);
    }

    void dragEnterEvent(QDragEnterEvent* event) override
    {
        if (event->source() == this) {
            QTreeWidget::dragEnterEvent(event); // 真实内部拖拽：基类接管指示器
            return;
        }
        // 外部/合成拖拽（无源对象）：基类 InternalMove 会因 source != this 直接拒收，
        // 导致 drop 不投递；这里只认自身模型格式并显式接受，落点判定在 dropEvent 完成
        if (event->mimeData()->hasFormat(
                QStringLiteral("application/x-qabstractitemmodeldatalist"))) {
            event->setDropAction(Qt::MoveAction);
            event->accept();
        }
    }

    void dragMoveEvent(QDragMoveEvent* event) override
    {
        if (event->source() == this) {
            QTreeWidget::dragMoveEvent(event);
            return;
        }
        if (event->mimeData()->hasFormat(
                QStringLiteral("application/x-qabstractitemmodeldatalist"))) {
            event->setDropAction(Qt::MoveAction);
            event->accept();
        }
    }

    void dropEvent(QDropEvent* event) override
    {
        // InternalMove 仅作交互外观：忽略动作，Qt 不改模型、startDrag 也不移除源行
        event->setDropAction(Qt::IgnoreAction);
        event->accept();
        if (!dropCallback
            || !event->mimeData()->hasFormat(
                QStringLiteral("application/x-qabstractitemmodeldatalist")))
            return; // 外部拖拽（如文件管理器）一律不接
        const QPoint pos = event->position().toPoint();
        QTreeWidgetItem* target = itemAt(pos);
        int indicator = dropIndicatorPosition();
        if (!event->source()) {
            // 合成事件（自动化测试，无真实拖拽源）：基类不更新指示器，按坐标判定
            indicator = indicatorAt(pos, target);
        }
        const QString targetId =
            target ? target->data(0, Qt::UserRole).toString() : QString();
        dropCallback(targetId, indicator);
    }

private:
    // 分支箭头区（有子行的行左端缩进区）：交给基类做展开/折叠
    bool isOnBranchIndicator(QTreeWidgetItem* item, const QPoint& pos) const
    {
        if (item->childCount() == 0)
            return false;
        int depth = 0;
        for (QTreeWidgetItem* p = item->parent(); p; p = p->parent())
            ++depth;
        return pos.x() < (depth + 1) * indentation();
    }

    // 与 Qt 边缘判定一致：行上/下边缘 2px 内视为行间，其余为行上
    int indicatorAt(const QPoint& pos, QTreeWidgetItem* target) const
    {
        if (!target)
            return QAbstractItemView::OnViewport;
        const QRect rect = visualItemRect(target);
        constexpr int margin = 2;
        if (pos.y() - rect.top() < margin)
            return QAbstractItemView::AboveItem;
        if (rect.bottom() - pos.y() < margin)
            return QAbstractItemView::BelowItem;
        return QAbstractItemView::OnItem;
    }

    QTreeWidgetItem* m_pressItem = nullptr;
    QPoint m_pressPos;
};

} // namespace

HierarchyPanel::HierarchyPanel(EditorCore* editorCore, QWidget* parent)
    : QWidget(parent)
    , m_editorCore(editorCore)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* tree = new HierarchyTreeWidget(this);
    tree->setObjectName(QStringLiteral("hierarchyTree")); // 供自动化测试定位
    tree->setHeaderLabel(QStringLiteral("名称"));
    // ExtendedSelection：Ctrl 多选；空白处拖动为橡皮筋框选
    tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    // InternalMove 仅作交互外观（拖拽手势 + 落点指示线）；drop 不交给 Qt 改模型，
    // 由 HierarchyTreeWidget::dropEvent 分流到 EditorCore::reparent 走命令
    tree->setDragDropMode(QAbstractItemView::InternalMove);
    tree->setDragEnabled(true);
    tree->setAcceptDrops(true);
    tree->setDropIndicatorShown(true);
    tree->setContextMenuPolicy(Qt::CustomContextMenu);
    tree->dropCallback = [this](const QString& targetId, int indicator) {
        onTreeDrop(targetId, indicator);
    };
    m_tree = tree;
    layout->addWidget(m_tree);

    connect(m_editorCore->runtimeData(), &RuntimeData::fileDataChanged,
            this, &HierarchyPanel::rebuildList);
    connect(m_editorCore->selection(), &SelectionModel::selectionChanged,
            this, &HierarchyPanel::syncFromSelectionModel);
    connect(m_tree, &QTreeWidget::itemSelectionChanged,
            this, &HierarchyPanel::onTreeSelectionChanged);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &HierarchyPanel::showContextMenu);

    rebuildList();
}

void HierarchyPanel::rebuildList()
{
    m_syncing = true;
    // 记录折叠状态（有子行但未展开的行）；新建的有子行的节点默认展开
    QSet<QString> collapsed;
    for (auto it = m_itemById.constBegin(); it != m_itemById.constEnd(); ++it) {
        if (it.value()->childCount() > 0 && !it.value()->isExpanded())
            collapsed.insert(it.key());
    }
    m_tree->clear();
    m_itemById.clear();

    const QList<ObjectData>& objects = m_editorCore->runtimeData()->fileData().objects;
    // 第一遍：按文件顺序建行（DFS 不变式保证兄弟顺序正确）
    for (const ObjectData& object : objects) {
        auto* item = new QTreeWidgetItem;
        item->setText(0, object.name); // 与 Inspector 一致
        item->setData(0, Qt::UserRole, object.id);
        if (object.color.isValid()) {
            QPixmap swatch(12, 12);
            swatch.fill(object.color);
            item->setIcon(0, QIcon(swatch));
        }
        m_itemById.insert(object.id, item);
    }
    // 第二遍：按 parentId 组装树（父缺失视为顶层，防御畸形数据）
    for (const ObjectData& object : objects) {
        QTreeWidgetItem* item = m_itemById.value(object.id);
        if (!item)
            continue;
        QTreeWidgetItem* parent =
            object.parentId.isEmpty() ? nullptr : m_itemById.value(object.parentId);
        if (parent)
            parent->addChild(item);
        else
            m_tree->addTopLevelItem(item);
    }
    // 兜底：环等畸形数据导致未挂树的行按顶层列出，不丢对象
    for (const ObjectData& object : objects) {
        QTreeWidgetItem* item = m_itemById.value(object.id);
        if (item && !item->treeWidget())
            m_tree->addTopLevelItem(item);
    }
    // 恢复折叠状态
    for (auto it = m_itemById.constBegin(); it != m_itemById.constEnd(); ++it) {
        if (it.value()->childCount() > 0)
            it.value()->setExpanded(!collapsed.contains(it.key()));
    }
    m_syncing = false;
    syncFromSelectionModel();
}

void HierarchyPanel::syncFromSelectionModel()
{
    if (m_syncing)
        return;
    m_syncing = true;
    m_tree->clearSelection();
    SelectionModel* selection = m_editorCore->selection();
    for (auto it = m_itemById.constBegin(); it != m_itemById.constEnd(); ++it) {
        if (selection->isSelected(it.key())) {
            // 选中子对象：自动展开父链并高亮对应行
            for (QTreeWidgetItem* p = it.value()->parent(); p; p = p->parent())
                p->setExpanded(true);
            it.value()->setSelected(true);
        }
    }
    m_syncing = false;
}

void HierarchyPanel::onTreeSelectionChanged()
{
    if (m_syncing)
        return;
    QList<QString> ids;
    const QList<QTreeWidgetItem*> items = m_tree->selectedItems();
    for (const QTreeWidgetItem* item : items)
        ids.append(item->data(0, Qt::UserRole).toString());
    m_syncing = true;
    m_editorCore->selection()->setSelection(ids);
    m_syncing = false;
}

void HierarchyPanel::onTreeDrop(const QString& targetId, int dropIndicator)
{
    const FileData& data = m_editorCore->runtimeData()->fileData();
    const QList<QString> selected = m_editorCore->selection()->selectedIds();
    if (selected.isEmpty())
        return;

    QHash<QString, QString> parents;
    for (const ObjectData& object : data.objects)
        parents.insert(object.id, object.parentId);
    const auto descendantOf = [&parents](const QString& id, const QString& ancestor) {
        QString p = parents.value(id);
        for (int guard = 0; !p.isEmpty() && guard <= parents.size(); ++guard) {
            if (p == ancestor)
                return true;
            p = parents.value(p);
        }
        return false;
    };

    // 拖动多个选中行时按当前顺序整体移动：移动根 = 选中集合中祖先未被选中的行
    const QSet<QString> selectedSet(selected.begin(), selected.end());
    QList<QString> roots;
    for (const ObjectData& object : data.objects) {
        if (!selectedSet.contains(object.id))
            continue;
        bool ancestorSelected = false;
        for (const QString& other : selectedSet) {
            if (other != object.id && descendantOf(object.id, other)) {
                ancestorSelected = true;
                break;
            }
        }
        if (!ancestorSelected)
            roots.append(object.id);
    }
    if (roots.isEmpty())
        return;

    // 被移动全集（含整棵子树）
    QSet<QString> moved;
    for (const ObjectData& object : data.objects) {
        for (const QString& root : roots) {
            if (object.id == root || descendantOf(object.id, root)) {
                moved.insert(object.id);
                break;
            }
        }
    }

    // 目标行在被移动集合内：落点相对自身无意义，忽略
    if (moved.contains(targetId))
        return;

    // 剩余列表中 parentId 的直系子对象（保持文件顺序）
    const auto siblingsRemaining = [&data, &moved](const QString& parentId) {
        QList<QString> siblings;
        for (const ObjectData& object : data.objects) {
            if (!moved.contains(object.id) && object.parentId == parentId)
                siblings.append(object.id);
        }
        return siblings;
    };

    QString newParentId;
    int insertIndex = 0;
    switch (dropIndicator) {
    case HierarchyTreeWidget::DropOnItem:
        // 落在行上 → 成为该行的子对象，排到该行子树末尾
        newParentId = targetId;
        insertIndex = siblingsRemaining(targetId).size();
        break;
    case HierarchyTreeWidget::DropAboveItem:
    case HierarchyTreeWidget::DropBelowItem: {
        // 落在行间 → 同级插入排序（父 = 目标行的父）
        const ObjectData* target = m_editorCore->findObject(targetId);
        if (!target)
            return;
        newParentId = target->parentId;
        const QList<QString> siblings = siblingsRemaining(newParentId);
        const int index = siblings.indexOf(targetId);
        if (index < 0)
            return;
        insertIndex = index + (dropIndicator == HierarchyTreeWidget::DropBelowItem ? 1 : 0);
        break;
    }
    case HierarchyTreeWidget::DropOnViewport:
        // 空白处 → 移到顶层末尾
        newParentId.clear();
        insertIndex = siblingsRemaining(QString()).size();
        break;
    default:
        return;
    }
    m_editorCore->reparent(roots, newParentId, insertIndex);
}

void HierarchyPanel::showContextMenu(const QPoint& pos)
{
    SelectionModel* selection = m_editorCore->selection();

    // 右键落在未选中行上：先单选该行，菜单操作即作用于它
    if (QTreeWidgetItem* item = m_tree->itemAt(pos)) {
        const QString id = item->data(0, Qt::UserRole).toString();
        if (!selection->isSelected(id))
            selection->select(id);
    }

    QMenu menu(this);
    QAction* createAction = menu.addAction(QStringLiteral("Create..."));
    menu.addSeparator();
    QAction* copyAction = menu.addAction(QStringLiteral("复制(&C)"));
    QAction* pasteAction = menu.addAction(QStringLiteral("粘贴(&P)"));
    QAction* deleteAction = menu.addAction(QStringLiteral("删除(&D)"));
    copyAction->setEnabled(!selection->isEmpty());
    deleteAction->setEnabled(!selection->isEmpty());
    pasteAction->setEnabled(m_editorCore->clipboardHasContent());

    QAction* chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
    if (!chosen)
        return;

    if (chosen == createAction) {
        CreateObjectDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted)
            m_editorCore->createObject(dialog.selectedType(), QPointF(0.0, 0.0)); // 落在 (0,0)
    } else if (chosen == copyAction) {
        m_editorCore->copySelection();
    } else if (chosen == pasteAction) {
        m_editorCore->pasteClipboard(); // 默认偏移 (x+20, y+20)
    } else if (chosen == deleteAction) {
        m_editorCore->deleteSelection();
    }
}
