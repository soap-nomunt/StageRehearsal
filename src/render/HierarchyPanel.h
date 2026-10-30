#pragma once

// Hierarchy 对象列表面板：演员、屏风、道具和空对象按 parentId 组装树（舞台不列出），
// 行序遵循"列表顺序 = 深度优先显示顺序"不变式；显示名字（与 Inspector 一致）+ 颜色块；
// 与视口选中双向同步（互发时防信号回环），选中子对象时自动展开父链；
// 行上长按拖拽做层级整理（drop 不交给 Qt 改模型，落点经 EditorCore::reparent 走命令），
// 框选只从空白处发起；空白处右键弹 Create/粘贴菜单（创建落在 (0,0)，粘贴走默认偏移）。

#include <QWidget>

#include <QHash>

class EditorCore;
class QTreeWidget;
class QTreeWidgetItem;

class HierarchyPanel : public QWidget {
    Q_OBJECT
public:
    explicit HierarchyPanel(EditorCore* editorCore, QWidget* parent = nullptr);

private:
    void rebuildList();             // fileDataChanged 触发，按数据层全量重建（保持选中/展开状态）
    void syncFromSelectionModel();  // 业务层选中 → 列表高亮（自动展开父链）
    void onTreeSelectionChanged();  // 列表高亮 → 业务层选中
    void showContextMenu(const QPoint& pos);
    void onTreeDrop(const QString& targetId, int dropIndicator); // 拖拽落点 → reparent 命令

    EditorCore* m_editorCore = nullptr; // 不拥有
    QTreeWidget* m_tree = nullptr;
    QHash<QString, QTreeWidgetItem*> m_itemById; // 对象 id → 树行（重建时更新）
    bool m_syncing = false;             // 双向同步防回环
};
