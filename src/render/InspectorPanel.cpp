#include "InspectorPanel.h"

#include "KeyframePrompt.h"
#include "business/EditorCore.h"
#include "business/SelectionModel.h"
#include "business/TimelinePlayer.h"
#include "data/RuntimeData.h"

#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <cmath>

namespace {

QDoubleSpinBox* makeCmSpinBox(QWidget* parent, double min, double max, double step)
{
    auto* spin = new QDoubleSpinBox(parent);
    spin->setRange(min, max);
    spin->setDecimals(1);
    spin->setSingleStep(step);
    spin->setSuffix(QStringLiteral(" cm"));
    spin->setKeyboardTracking(false); // 输入回车/失焦才提交，步进按钮即时提交
    return spin;
}

QDoubleSpinBox* makeRotationSpinBox(QWidget* parent)
{
    auto* spin = new QDoubleSpinBox(parent);
    spin->setRange(0.0, 359.9); // 需求角度制，归一化到 [0,360)
    spin->setDecimals(1);
    spin->setSingleStep(1.0);
    spin->setWrapping(true);
    spin->setSuffix(QStringLiteral(" °"));
    spin->setKeyboardTracking(false);
    return spin;
}

double normalizedRotation(double degrees)
{
    degrees = std::fmod(degrees, 360.0);
    return degrees < 0.0 ? degrees + 360.0 : degrees;
}

} // namespace

InspectorPanel::InspectorPanel(EditorCore* editorCore, QWidget* parent)
    : QWidget(parent)
    , m_editorCore(editorCore)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);

    m_stack = new QStackedWidget(this);
    m_stack->setObjectName(QStringLiteral("inspectorStack")); // 供自动化测试定位
    layout->addWidget(m_stack);

    // ---- 占位页：多选或无选中 ----
    m_emptyLabel = new QLabel(this);
    m_emptyLabel->setObjectName(QStringLiteral("inspectorEmptyLabel"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_stack->addWidget(m_emptyLabel);

    // ---- 演员页：角色名/位置/朝向/颜色 ----
    auto* actorPage = new QWidget(this);
    auto* actorForm = new QFormLayout(actorPage);
    m_actorName = new QLineEdit(actorPage);
    m_actorX = makeCmSpinBox(actorPage, -99999.9, 99999.9, 10.0);
    m_actorY = makeCmSpinBox(actorPage, -99999.9, 99999.9, 10.0);
    m_actorRotation = makeRotationSpinBox(actorPage);
    m_actorColor = new QPushButton(actorPage);
    m_actorName->setObjectName(QStringLiteral("actorNameEdit"));
    m_actorX->setObjectName(QStringLiteral("actorXSpin"));
    m_actorY->setObjectName(QStringLiteral("actorYSpin"));
    m_actorRotation->setObjectName(QStringLiteral("actorRotSpin"));
    m_actorColor->setObjectName(QStringLiteral("actorColorButton"));
    m_actorColor->setFixedHeight(24);
    actorForm->addRow(QStringLiteral("角色名"), m_actorName);
    actorForm->addRow(QStringLiteral("位置 X"), m_actorX);
    actorForm->addRow(QStringLiteral("位置 Y"), m_actorY);
    actorForm->addRow(QStringLiteral("朝向"), m_actorRotation);
    actorForm->addRow(QStringLiteral("颜色"), m_actorColor);
    m_stack->addWidget(actorPage);

    // ---- 屏风页：屏风名/位置/大小/朝向/颜色 ----
    auto* screenPage = new QWidget(this);
    auto* screenForm = new QFormLayout(screenPage);
    m_screenName = new QLineEdit(screenPage);
    m_screenX = makeCmSpinBox(screenPage, -99999.9, 99999.9, 10.0);
    m_screenY = makeCmSpinBox(screenPage, -99999.9, 99999.9, 10.0);
    m_screenW = makeCmSpinBox(screenPage, 1.0, 9999.9, 10.0);
    m_screenH = makeCmSpinBox(screenPage, 1.0, 9999.9, 10.0);
    m_screenRotation = makeRotationSpinBox(screenPage);
    m_screenColor = new QPushButton(screenPage);
    m_screenName->setObjectName(QStringLiteral("screenNameEdit"));
    m_screenX->setObjectName(QStringLiteral("screenXSpin"));
    m_screenY->setObjectName(QStringLiteral("screenYSpin"));
    m_screenW->setObjectName(QStringLiteral("screenWSpin"));
    m_screenH->setObjectName(QStringLiteral("screenHSpin"));
    m_screenRotation->setObjectName(QStringLiteral("screenRotSpin"));
    m_screenColor->setObjectName(QStringLiteral("screenColorButton"));
    m_screenColor->setFixedHeight(24);
    screenForm->addRow(QStringLiteral("屏风名"), m_screenName);
    screenForm->addRow(QStringLiteral("位置 X"), m_screenX);
    screenForm->addRow(QStringLiteral("位置 Y"), m_screenY);
    screenForm->addRow(QStringLiteral("大小 宽"), m_screenW);
    screenForm->addRow(QStringLiteral("大小 高"), m_screenH);
    screenForm->addRow(QStringLiteral("朝向"), m_screenRotation);
    screenForm->addRow(QStringLiteral("颜色"), m_screenColor);
    m_stack->addWidget(screenPage);

    // ---- 空对象页：只有名称 ----
    auto* groupPage = new QWidget(this);
    auto* groupForm = new QFormLayout(groupPage);
    m_groupName = new QLineEdit(groupPage);
    m_groupName->setObjectName(QStringLiteral("groupNameEdit"));
    groupForm->addRow(QStringLiteral("名称"), m_groupName);
    m_stack->addWidget(groupPage);

    // 修改立即生效：提交即走命令
    connect(m_actorName, &QLineEdit::editingFinished, this, &InspectorPanel::commitName);
    connect(m_screenName, &QLineEdit::editingFinished, this, &InspectorPanel::commitName);
    connect(m_groupName, &QLineEdit::editingFinished, this, &InspectorPanel::commitName);
    for (QDoubleSpinBox* spin : { m_actorX, m_actorY, m_actorRotation,
                                  m_screenX, m_screenY, m_screenRotation }) {
        connect(spin, &QDoubleSpinBox::valueChanged, this, &InspectorPanel::commitTransform);
    }
    connect(m_screenW, &QDoubleSpinBox::valueChanged, this, &InspectorPanel::commitSize);
    connect(m_screenH, &QDoubleSpinBox::valueChanged, this, &InspectorPanel::commitSize);
    connect(m_actorColor, &QPushButton::clicked, this, &InspectorPanel::pickColor);
    connect(m_screenColor, &QPushButton::clicked, this, &InspectorPanel::pickColor);

    connect(m_editorCore->selection(), &SelectionModel::selectionChanged,
            this, &InspectorPanel::refresh);
    connect(m_editorCore->runtimeData(), &RuntimeData::fileDataChanged,
            this, &InspectorPanel::refresh);
    // 游标移动/播放 tick 后，位置/朝向字段跟随当前时刻插值结果刷新
    connect(m_editorCore->timelinePlayer(), &TimelinePlayer::currentTimeChanged,
            this, [this](double) { refresh(); });

    refresh();
}

const ObjectData* InspectorPanel::currentObject() const
{
    const QString id = m_editorCore->selection()->singleSelectedId();
    return id.isEmpty() ? nullptr : m_editorCore->findObject(id);
}

void InspectorPanel::refresh()
{
    if (m_updating)
        return;
    m_updating = true;

    const ObjectData* object = currentObject();
    if (!object) {
        // 多选或无选中：清空表单
        const int count = m_editorCore->selection()->count();
        m_emptyLabel->setText(count > 1
                                  ? QStringLiteral("已选中 %1 个对象").arg(count)
                                  : QStringLiteral("未选中对象"));
        m_stack->setCurrentWidget(m_emptyLabel);
        m_updating = false;
        return;
    }

    if (object->type == ObjectType::Group) {
        // 空对象：纯组织节点，只有名称表单
        m_groupName->setText(object->name);
        m_stack->setCurrentIndex(3);
    } else if (object->type == ObjectType::Screen) {
        // 位置/朝向显示当前时刻插值结果（显示值与数据值分离）
        const Transform shown =
            TimelinePlayer::interpolate(*object, m_editorCore->timelinePlayer()->currentTime());
        m_screenName->setText(object->name);
        m_screenX->setValue(shown.x);
        m_screenY->setValue(shown.y);
        m_screenW->setValue(object->size ? object->size->width() : 100.0);
        m_screenH->setValue(object->size ? object->size->height() : 10.0);
        m_screenRotation->setValue(normalizedRotation(shown.rotation));
        updateColorButton(m_screenColor, object->color);
        m_stack->setCurrentIndex(2);
    } else {
        const Transform shown =
            TimelinePlayer::interpolate(*object, m_editorCore->timelinePlayer()->currentTime());
        m_actorName->setText(object->name);
        m_actorX->setValue(shown.x);
        m_actorY->setValue(shown.y);
        m_actorRotation->setValue(normalizedRotation(shown.rotation));
        updateColorButton(m_actorColor, object->color);
        m_stack->setCurrentIndex(1);
    }

    m_updating = false;
}

void InspectorPanel::commitName()
{
    if (m_updating)
        return;
    const ObjectData* object = currentObject();
    if (!object)
        return;
    QLineEdit* edit = object->type == ObjectType::Screen ? m_screenName
        : object->type == ObjectType::Group              ? m_groupName
                                                         : m_actorName;
    const QString name = edit->text().trimmed();
    if (name == object->name)
        return;
    if (name.isEmpty() || m_editorCore->isNameTaken(name, object->id)) {
        QMessageBox::warning(this, QStringLiteral("重命名失败"),
                             name.isEmpty()
                                 ? QStringLiteral("名字不能为空。")
                                 : QStringLiteral("名字“%1”已被占用：演员与屏风的名字必须唯一。").arg(name));
        edit->setText(object->name); // 恢复原名
        return;
    }
    m_editorCore->setObjectName(object->id, name);
}

void InspectorPanel::commitTransform()
{
    if (m_updating)
        return;
    const ObjectData* object = currentObject();
    if (!object || object->type == ObjectType::Group)
        return;
    // 数值框持有的是当前时刻显示值；提交游标时刻语义（T 处有帧更新、无帧询问打帧）
    Transform value;
    if (object->type == ObjectType::Screen) {
        value.x = m_screenX->value();
        value.y = m_screenY->value();
        value.rotation = m_screenRotation->value();
    } else {
        value.x = m_actorX->value();
        value.y = m_actorY->value();
        value.rotation = m_actorRotation->value();
    }
    const auto confirm = [this] {
        return KeyframePrompt::confirmAtCursor(
            this, m_editorCore->runtimeData()->settings(),
            m_editorCore->timelinePlayer()->currentTime());
    };
    if (!m_editorCore->setTransformsAtCursor({ { object->id, value } }, confirm))
        refresh(); // 无变更/选"否"：数值框回弹到当前时刻显示值
}

void InspectorPanel::commitSize()
{
    if (m_updating)
        return;
    const ObjectData* object = currentObject();
    if (!object || object->type != ObjectType::Screen)
        return;
    m_editorCore->setScreenSize(object->id, QSizeF(m_screenW->value(), m_screenH->value()));
}

void InspectorPanel::pickColor()
{
    const ObjectData* object = currentObject();
    if (!object)
        return;
    const QColor chosen = QColorDialog::getColor(object->color, this, QStringLiteral("选择颜色"));
    if (chosen.isValid())
        m_editorCore->setObjectColor(object->id, chosen);
}

void InspectorPanel::updateColorButton(QPushButton* button, const QColor& color) const
{
    button->setStyleSheet(QStringLiteral("background-color: %1; border: 1px solid #202020;")
                              .arg(color.isValid() ? color.name() : QStringLiteral("#808080")));
}
