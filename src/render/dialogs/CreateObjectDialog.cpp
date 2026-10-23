#include "CreateObjectDialog.h"

#include <QDialogButtonBox>
#include <QLabel>
#include <QRadioButton>
#include <QVBoxLayout>

CreateObjectDialog::CreateObjectDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("创建对象"));

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel(QStringLiteral("选择对象类型："), this));

    m_actorRadio = new QRadioButton(QStringLiteral("演员"), this);
    m_screenRadio = new QRadioButton(QStringLiteral("屏风"), this);
    m_groupRadio = new QRadioButton(QStringLiteral("空对象"), this);
    m_propRadio = new QRadioButton(QStringLiteral("舞台道具"), this); // 阶段13 起可选
    m_propRadio->setObjectName(QStringLiteral("propRadio")); // 供自测定位
    m_actorRadio->setChecked(true);

    layout->addWidget(m_actorRadio);
    layout->addWidget(m_screenRadio);
    layout->addWidget(m_groupRadio);
    layout->addWidget(m_propRadio);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);
}

ObjectType CreateObjectDialog::selectedType() const
{
    if (m_propRadio->isChecked())
        return ObjectType::Prop;
    if (m_groupRadio->isChecked())
        return ObjectType::Group;
    return m_screenRadio->isChecked() ? ObjectType::Screen : ObjectType::Actor;
}
