#include "InspectorPanel.h"

#include "KeyframePrompt.h"
#include "business/EditorCore.h"
#include "business/PathCommands.h" // resolveSegmentEndpoints
#include "business/PathGeometry.h"
#include "business/SelectionModel.h"
#include "business/TimelinePlayer.h"
#include "data/RuntimeData.h"

#include <QColorDialog>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
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

    // ---- 路径页（阶段12）：名称（派生只读）+ 连线类型 + 圆心角行 + 路径点 t +
    // 只读段时长/弧长；一切提交走命令入撤销栈 ----
    m_pathPage = new QWidget(this);
    {
        auto* pathLayout = new QVBoxLayout(m_pathPage);
        pathLayout->setContentsMargins(4, 4, 4, 4);
        pathLayout->setSpacing(4);
        // 行 = 标题小字 + 控件（与既有表单的紧凑风格一致）
        const auto addRow = [this, pathLayout](const QString& title, QWidget* control) {
            auto* row = new QWidget(m_pathPage);
            auto* rowLayout = new QVBoxLayout(row);
            rowLayout->setContentsMargins(0, 2, 0, 2);
            rowLayout->setSpacing(2);
            auto* label = new QLabel(title, row);
            label->setStyleSheet(QStringLiteral("color: #888888;"));
            rowLayout->addWidget(label);
            rowLayout->addWidget(control);
            pathLayout->addWidget(row);
        };

        m_pathName = new QLineEdit(m_pathPage);
        m_pathName->setObjectName(QStringLiteral("pathNameEdit"));
        m_pathName->setReadOnly(true); // 名称派生自段标识，不可修改
        addRow(QStringLiteral("名称"), m_pathName);

        m_pathType = new QComboBox(m_pathPage);
        m_pathType->setObjectName(QStringLiteral("pathTypeCombo"));
        m_pathType->addItem(QStringLiteral("直线"));
        m_pathType->addItem(QStringLiteral("弧线"));
        addRow(QStringLiteral("连线类型"), m_pathType);

        // 圆心角行：动态生成（无路径点 1 行 / 有路径点 N+1 行），仅弧线可编辑
        auto* anglesCaption = new QLabel(QStringLiteral("圆心角（度，正 = 沿行进方向左侧鼓出）"),
                                         m_pathPage);
        anglesCaption->setStyleSheet(QStringLiteral("color: #888888;"));
        pathLayout->addWidget(anglesCaption);
        auto* anglesContainer = new QWidget(m_pathPage);
        m_pathAnglesLayout = new QVBoxLayout(anglesContainer);
        m_pathAnglesLayout->setContentsMargins(0, 0, 0, 0);
        m_pathAnglesLayout->setSpacing(2);
        pathLayout->addWidget(anglesContainer);

        // 选中路径点 t（沿路径弧长百分比）：无选中点时整行隐藏
        m_pathPointT = new QDoubleSpinBox(m_pathPage);
        m_pathPointT->setObjectName(QStringLiteral("pathPointTSpin"));
        m_pathPointT->setRange(0.0, 1.0);
        m_pathPointT->setDecimals(3);
        m_pathPointT->setSingleStep(0.05);
        m_pathPointT->setKeyboardTracking(false); // 输入回车/失焦才提交
        m_pathPointTRow = new QWidget(m_pathPage);
        {
            auto* rowLayout = new QVBoxLayout(m_pathPointTRow);
            rowLayout->setContentsMargins(0, 2, 0, 2);
            rowLayout->setSpacing(2);
            auto* label = new QLabel(QStringLiteral("路径点 t（弧长百分比）"), m_pathPointTRow);
            label->setStyleSheet(QStringLiteral("color: #888888;"));
            rowLayout->addWidget(label);
            rowLayout->addWidget(m_pathPointT);
        }
        pathLayout->addWidget(m_pathPointTRow);

        m_pathMetrics = new QLabel(m_pathPage);
        m_pathMetrics->setObjectName(QStringLiteral("pathMetricsLabel"));
        m_pathMetrics->setStyleSheet(QStringLiteral("color: #888888;"));
        pathLayout->addWidget(m_pathMetrics);
        pathLayout->addStretch();
    }
    m_stack->addWidget(m_pathPage); // 第五页（index 4）

    // ---- 道具页（阶段13，第六页 index 5）：名称/位置/朝向/颜色 + 缩放 + 绑定 +
    // 可供站立 + 编辑形状（占位禁用）+ 只读实际尺寸 ----
    m_propPage = new QWidget(this);
    auto* propForm = new QFormLayout(m_propPage);
    m_propName = new QLineEdit(m_propPage);
    m_propX = makeCmSpinBox(m_propPage, -99999.9, 99999.9, 10.0);
    m_propY = makeCmSpinBox(m_propPage, -99999.9, 99999.9, 10.0);
    m_propRotation = makeRotationSpinBox(m_propPage);
    m_propColor = new QPushButton(m_propPage);
    m_propScaleX = new QDoubleSpinBox(m_propPage);
    m_propScaleY = new QDoubleSpinBox(m_propPage);
    for (QDoubleSpinBox* spin : { m_propScaleX, m_propScaleY }) {
        spin->setRange(0.1, 100.0); // 倍数制：1 = 无缩放；范围 0.1~100 仅正值
        spin->setDecimals(1);
        spin->setSingleStep(0.1);
        spin->setSuffix(QStringLiteral(" ×"));
        spin->setKeyboardTracking(false);
    }
    m_propScaleLink = new QPushButton(QStringLiteral("绑定 X/Y"), m_propPage);
    m_propScaleLink->setObjectName(QStringLiteral("propScaleLinkButton"));
    m_propScaleLink->setCheckable(true);
    m_propScaleLink->setToolTip(
        QStringLiteral("绑定后修改一方，另一方同步增减相同增量（切换本身不改值）"));
    m_propStandable = new QCheckBox(QStringLiteral("可供站立"), m_propPage);
    m_propStandable->setObjectName(QStringLiteral("propStandableCheck"));
    m_propStandable->setToolTip(
        QStringLiteral("取消勾选后，该道具成为阻挡物：路径编辑不允许穿过它"));
    m_propEditShape = new QPushButton(QStringLiteral("编辑形状…"), m_propPage);
    m_propEditShape->setObjectName(QStringLiteral("propEditShapeButton"));
    m_propEditShape->setEnabled(false); // 形状编辑器属后续任务，本轮占位禁用
    m_propEditShape->setToolTip(QStringLiteral("形状编辑器将在后续版本提供"));
    m_propActualSize = new QLabel(m_propPage);
    m_propActualSize->setObjectName(QStringLiteral("propActualSizeLabel"));
    m_propActualSize->setStyleSheet(QStringLiteral("color: #888888;"));

    m_propName->setObjectName(QStringLiteral("propNameEdit"));
    m_propX->setObjectName(QStringLiteral("propXSpin"));
    m_propY->setObjectName(QStringLiteral("propYSpin"));
    m_propRotation->setObjectName(QStringLiteral("propRotSpin"));
    m_propColor->setObjectName(QStringLiteral("propColorButton"));
    m_propScaleX->setObjectName(QStringLiteral("propScaleXSpin"));
    m_propScaleY->setObjectName(QStringLiteral("propScaleYSpin"));
    m_propColor->setFixedHeight(24);
    propForm->addRow(QStringLiteral("道具名"), m_propName);
    propForm->addRow(QStringLiteral("位置 X"), m_propX);
    propForm->addRow(QStringLiteral("位置 Y"), m_propY);
    propForm->addRow(QStringLiteral("朝向"), m_propRotation);
    propForm->addRow(QStringLiteral("颜色"), m_propColor);
    propForm->addRow(QStringLiteral("缩放 X"), m_propScaleX);
    propForm->addRow(QStringLiteral("缩放 Y"), m_propScaleY);
    propForm->addRow(QString(), m_propScaleLink);
    propForm->addRow(QStringLiteral("站立"), m_propStandable);
    propForm->addRow(QStringLiteral("形状"), m_propEditShape);
    propForm->addRow(QStringLiteral("实际尺寸"), m_propActualSize);
    m_stack->addWidget(m_propPage);

    // 修改立即生效：提交即走命令
    connect(m_actorName, &QLineEdit::editingFinished, this, &InspectorPanel::commitName);
    connect(m_screenName, &QLineEdit::editingFinished, this, &InspectorPanel::commitName);
    connect(m_groupName, &QLineEdit::editingFinished, this, &InspectorPanel::commitName);
    connect(m_propName, &QLineEdit::editingFinished, this, &InspectorPanel::commitName);
    for (QDoubleSpinBox* spin : { m_actorX, m_actorY, m_actorRotation,
                                  m_screenX, m_screenY, m_screenRotation,
                                  m_propX, m_propY, m_propRotation }) {
        connect(spin, &QDoubleSpinBox::valueChanged, this, &InspectorPanel::commitTransform);
    }
    connect(m_screenW, &QDoubleSpinBox::valueChanged, this, &InspectorPanel::commitSize);
    connect(m_screenH, &QDoubleSpinBox::valueChanged, this, &InspectorPanel::commitSize);
    connect(m_actorColor, &QPushButton::clicked, this, &InspectorPanel::pickColor);
    connect(m_screenColor, &QPushButton::clicked, this, &InspectorPanel::pickColor);
    connect(m_propColor, &QPushButton::clicked, this, &InspectorPanel::pickColor);
    // 阶段13：道具缩放/站立提交
    connect(m_propScaleX, &QDoubleSpinBox::valueChanged, this, &InspectorPanel::commitPropScale);
    connect(m_propScaleY, &QDoubleSpinBox::valueChanged, this, &InspectorPanel::commitPropScale);
    connect(m_propStandable, &QCheckBox::toggled, this, &InspectorPanel::commitPropStandable);
    // 阶段12：路径页提交（一切走命令入撤销栈；currentIndexChanged 同 spin 的
    // valueChanged 模式：程序回显走 m_updating 屏蔽，用户切换才提交）
    connect(m_pathType, &QComboBox::currentIndexChanged,
            this, &InspectorPanel::commitPathType);
    connect(m_pathPointT, &QDoubleSpinBox::valueChanged,
            this, &InspectorPanel::commitPathPointT);

    connect(m_editorCore->selection(), &SelectionModel::selectionChanged,
            this, &InspectorPanel::refresh);
    connect(m_editorCore->runtimeData(), &RuntimeData::fileDataChanged,
            this, &InspectorPanel::refresh);
    // 阶段12：选中路径变化 → 切路径页/回对象页
    connect(m_editorCore, &EditorCore::activePathChanged,
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

    // 阶段12：选中路径优先于对象表单（点选白线 → 路径页）
    if (m_editorCore->activePath()) {
        refreshPathPage();
        m_stack->setCurrentWidget(m_pathPage);
        m_updating = false;
        return;
    }
    m_pathAngleSegmentValid = false; // 离开路径页：圆心角行结构缓存失效

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
    } else if (object->type == ObjectType::Prop) {
        // 道具（阶段13）：位置/朝向显示当前时刻插值结果；缩放/站立/尺寸为静态属性
        const Transform shown =
            TimelinePlayer::interpolate(*object, m_editorCore->runtimeData()->fileData().paths,
                                        m_editorCore->timelinePlayer()->currentTime());
        m_propName->setText(object->name);
        m_propX->setValue(shown.x);
        m_propY->setValue(shown.y);
        m_propRotation->setValue(normalizedRotation(shown.rotation));
        updateColorButton(m_propColor, object->color);
        m_propScaleX->setValue(object->scaleX);
        m_propScaleY->setValue(object->scaleY);
        m_propStandable->setChecked(object->standable);
        m_propActualSize->setText(
            QStringLiteral("宽 %1 cm × 深 %2 cm")
                .arg(propShapeWidth(*object) * object->scaleX, 0, 'f', 1)
                .arg(propShapeDepth(*object) * object->scaleY, 0, 'f', 1));
        m_stack->setCurrentWidget(m_propPage);
    } else if (object->type == ObjectType::Screen) {
        // 位置/朝向显示当前时刻插值结果（显示值与数据值分离；阶段12 起含路径）
        const Transform shown =
            TimelinePlayer::interpolate(*object, m_editorCore->runtimeData()->fileData().paths,
                                        m_editorCore->timelinePlayer()->currentTime());
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
            TimelinePlayer::interpolate(*object, m_editorCore->runtimeData()->fileData().paths,
                                        m_editorCore->timelinePlayer()->currentTime());
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
        : object->type == ObjectType::Prop               ? m_propName
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
    } else if (object->type == ObjectType::Prop) {
        value.x = m_propX->value();
        value.y = m_propY->value();
        value.rotation = m_propRotation->value();
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

// ---- 阶段12：路径页 ----

void InspectorPanel::refreshPathPage()
{
    const auto& activePath = *m_editorCore->activePath();
    const SegmentRef& segment = activePath.segment;
    const ObjectData* object = m_editorCore->findObject(segment.objectId);
    if (!object)
        return; // 防御（pruneActivePath 会清除失效路径）
    const PathData* path =
        m_editorCore->findPath(segment.objectId, segment.from, segment.to);
    const PathType type = path ? path->type : PathType::Line;
    const int pointCount = path ? path->points.size() : 0;

    // 名称派生只读：<对象名>_<起>s至<止>s
    m_pathName->setText(QStringLiteral("%1_%2s至%3s")
                            .arg(object->name, QString::number(segment.from),
                                 QString::number(segment.to)));
    m_pathType->setCurrentIndex(type == PathType::Arc ? 1 : 0);

    // 圆心角行：结构（段/行数）不变只刷新值，避免输入中重建抖动
    rebuildPathAngleRows(pointCount + 1);
    for (int i = 0; i < m_pathAngleSpins.size(); ++i) {
        m_pathAngleSpins.at(i)->setEnabled(type == PathType::Arc);
        m_pathAngleSpins.at(i)->setValue(
            path && i < path->legAngles.size() ? path->legAngles.at(i) : 0.0);
    }

    // 端点（两帧数据值）与当前复合曲线：段时长/弧长只读显示、选中点 t 回显
    QPointF fromPos;
    QPointF toPos;
    if (resolveSegmentEndpoints(m_editorCore->runtimeData()->fileData(), segment.objectId,
                                segment.from, segment.to, fromPos, toPos)) {
        const QList<QPointF> polyline = path
            ? PathGeometry::samplePath(*path, fromPos, toPos)
            : QList<QPointF> { fromPos, toPos };
        m_pathMetrics->setText(QStringLiteral("段时长 %1 s，路径弧长 %2 cm")
                                   .arg(segment.to - segment.from, 0, 'f', 1)
                                   .arg(PathGeometry::totalLength(polyline), 0, 'f', 1));
        if (activePath.pointIndex >= 0 && activePath.pointIndex < pointCount) {
            const PathPoint& point = path->points.at(activePath.pointIndex);
            m_pathPointT->setValue(PathGeometry::projectToRatio(
                polyline, QPointF(point.x, point.y))); // 拖拽后回显投影弧长百分比
            m_pathPointTRow->setVisible(true);
        } else {
            m_pathPointTRow->setVisible(false);
        }
    } else {
        m_pathMetrics->clear();
        m_pathPointTRow->setVisible(false);
    }
}

void InspectorPanel::rebuildPathAngleRows(int legCount)
{
    const auto& segment = m_editorCore->activePath()->segment;
    if (m_pathAngleSegmentValid && m_pathAngleSegment == segment
        && m_pathAngleLegCount == legCount)
        return; // 结构未变：只刷新值（refreshPathPage 负责）
    m_pathAngleSegment = segment;
    m_pathAngleSegmentValid = true;
    m_pathAngleLegCount = legCount;

    // 清空旧行
    m_pathAngleLabels.clear();
    m_pathAngleSpins.clear();
    while (QLayoutItem* item = m_pathAnglesLayout->takeAt(0)) {
        delete item->widget();
        delete item;
    }

    // 段端点名：起点 / P1…PN / 终点
    const auto pointName = [legCount](int index) {
        if (index == 0)
            return QStringLiteral("起点");
        if (index >= legCount)
            return QStringLiteral("终点");
        return QStringLiteral("P%1").arg(index);
    };
    for (int i = 0; i < legCount; ++i) {
        auto* row = new QWidget(m_pathAnglesLayout->parentWidget());
        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(0, 0, 0, 0);
        auto* label = new QLabel(
            QStringLiteral("段%1 %2→%3").arg(i + 1).arg(pointName(i), pointName(i + 1)),
            row);
        auto* spin = new QDoubleSpinBox(row);
        spin->setObjectName(QStringLiteral("pathLegAngleSpin%1").arg(i));
        spin->setRange(-PathGeometry::kMaxLegAngleDeg, PathGeometry::kMaxLegAngleDeg);
        spin->setDecimals(1);
        spin->setSingleStep(5.0);
        spin->setSuffix(QStringLiteral(" °"));
        spin->setKeyboardTracking(false);
        rowLayout->addWidget(label);
        rowLayout->addWidget(spin);
        m_pathAnglesLayout->addWidget(row);
        m_pathAngleLabels.append(label);
        m_pathAngleSpins.append(spin);
        connect(spin, &QDoubleSpinBox::valueChanged, this, [this, i](double value) {
            if (m_updating)
                return;
            const auto& activePath = *m_editorCore->activePath();
            m_editorCore->setPathLegAngle(activePath.segment.objectId,
                                          activePath.segment.from, activePath.segment.to,
                                          i, value);
        });
    }
}

void InspectorPanel::commitPathType(int index)
{
    if (m_updating || !m_editorCore->activePath())
        return;
    const auto& segment = m_editorCore->activePath()->segment;
    m_editorCore->setPathType(segment.objectId, segment.from, segment.to,
                              index == 1 ? PathType::Arc : PathType::Line);
}

void InspectorPanel::commitPathPointT()
{
    if (m_updating || !m_editorCore->activePath())
        return;
    const auto& activePath = *m_editorCore->activePath();
    if (activePath.pointIndex < 0)
        return;
    m_editorCore->setPathPointT(activePath.segment.objectId, activePath.segment.from,
                                activePath.segment.to, activePath.pointIndex,
                                m_pathPointT->value());
}

// ---- 阶段13：道具页 ----

void InspectorPanel::commitPropScale()
{
    if (m_updating)
        return;
    const ObjectData* object = currentObject();
    if (!object || object->type != ObjectType::Prop)
        return;
    double targetX = m_propScaleX->value();
    double targetY = m_propScaleY->value();
    if (m_propScaleLink->isChecked()) {
        // 绑定（决策4）：一方改动多少、另一方同步增减同一增量（非等比锁比值）；
        // 切换开关本身不改值
        auto* source = qobject_cast<QDoubleSpinBox*>(sender());
        if (source == m_propScaleX)
            targetY = qBound(0.1, object->scaleY + (targetX - object->scaleX), 100.0);
        else if (source == m_propScaleY)
            targetX = qBound(0.1, object->scaleX + (targetY - object->scaleY), 100.0);
    }
    m_editorCore->setPropScale(object->id, targetX, targetY); // 无变更不入栈
}

void InspectorPanel::commitPropStandable()
{
    if (m_updating)
        return;
    const ObjectData* object = currentObject();
    if (!object || object->type != ObjectType::Prop)
        return;
    m_editorCore->setPropStandable(object->id, m_propStandable->isChecked());
}

// 未缩放形状包围盒宽/深（缺省 = 默认 100×100 矩形）
double InspectorPanel::propShapeWidth(const ObjectData& object) const
{
    const PropShape shape = object.shape.value_or(defaultPropShape());
    double lo = 0.0, hi = 0.0;
    for (const PathPoint& v : shape.vertices) {
        lo = qMin(lo, v.x);
        hi = qMax(hi, v.x);
    }
    return hi - lo;
}

double InspectorPanel::propShapeDepth(const ObjectData& object) const
{
    const PropShape shape = object.shape.value_or(defaultPropShape());
    double lo = 0.0, hi = 0.0;
    for (const PathPoint& v : shape.vertices) {
        lo = qMin(lo, v.y);
        hi = qMax(hi, v.y);
    }
    return hi - lo;
}
