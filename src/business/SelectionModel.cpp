#include "SelectionModel.h"

SelectionModel::SelectionModel(QObject* parent)
    : QObject(parent)
{
}

QString SelectionModel::singleSelectedId() const
{
    return m_ids.size() == 1 ? m_ids.first() : QString();
}

void SelectionModel::setSelection(const QList<QString>& ids)
{
    QList<QString> cleaned;
    for (const QString& id : ids) {
        if (!id.isEmpty() && !cleaned.contains(id))
            cleaned.append(id);
    }
    if (cleaned == m_ids)
        return;
    m_ids = cleaned;
    emit selectionChanged();
}

void SelectionModel::select(const QString& id)
{
    setSelection(id.isEmpty() ? QList<QString>() : QList<QString> { id });
}

void SelectionModel::toggle(const QString& id)
{
    if (id.isEmpty())
        return;
    QList<QString> next = m_ids;
    if (next.contains(id))
        next.removeAll(id);
    else
        next.append(id);
    setSelection(next);
}

void SelectionModel::clear()
{
    setSelection(QList<QString>());
}
