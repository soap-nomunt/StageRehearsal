#pragma once

// 选择模型：有序选中 id 列表。单选 / Ctrl 切换 / 框选批量设置 / 清空。
// 选中状态以对象 id 标识：场景全量重建后按 id 恢复选中高亮。
// 变化时发 selectionChanged()（内容未变不发）。

#include <QList>
#include <QObject>
#include <QString>

class SelectionModel : public QObject {
    Q_OBJECT
public:
    explicit SelectionModel(QObject* parent = nullptr);

    const QList<QString>& selectedIds() const { return m_ids; }
    int count() const { return m_ids.size(); }
    bool isEmpty() const { return m_ids.isEmpty(); }
    bool isSelected(const QString& id) const { return m_ids.contains(id); }

    // 仅单选时返回该 id，否则返回空串
    QString singleSelectedId() const;

public slots:
    void setSelection(const QList<QString>& ids); // 批量设置（框选）：去重、保序、去空串
    void select(const QString& id);               // 单选
    void toggle(const QString& id);               // Ctrl 切换
    void clear();

signals:
    void selectionChanged();

private:
    QList<QString> m_ids;
};
