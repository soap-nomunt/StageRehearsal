#include "StageView.h"

#include "KeyframePrompt.h"
#include "RenderUtil.h"
#include "StageObjectItem.h"
#include "StageScene.h"
#include "business/EditorCore.h"
#include "business/PathCommands.h" // resolveSegmentEndpoints
#include "business/PathGeometry.h"
#include "business/SelectionModel.h"
#include "business/TimelinePlayer.h"
#include "data/AppSettings.h"
#include "data/RuntimeData.h"
#include "dialogs/CreateObjectDialog.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineF>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QRubberBand>
#include <QScreen>
#include <QScrollBar>
#include <QWheelEvent>

#include <cmath>

namespace {

// 旋转吸附：拖拽朝向手柄时吸附到 AppSettings::rotationSnapDegrees 的整数倍，
// 归一化到 [0,360)
double snappedRotation(double degrees)
{
    const double step = AppSettings::rotationSnapDegrees;
    double snapped = std::round(degrees / step) * step;
    snapped = std::fmod(snapped, 360.0);
    return snapped < 0.0 ? snapped + 360.0 : snapped;
}

} // namespace

StageView::StageView(StageScene* scene, EditorCore* editorCore, QWidget* parent)
    : QGraphicsView(scene, parent)
    , m_scene(scene)
    , m_editorCore(editorCore)
{
    // 缩放以鼠标位置为中心
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setRenderHint(QPainter::Antialiasing);
    setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
    setDragMode(QGraphicsView::NoDrag); // 框选手绘橡皮筋（中心点判定），不用内置相交判定
    setFocusPolicy(Qt::StrongFocus);    // 方向键微调需要键盘焦点
    viewport()->setMouseTracking(true); // 无按键移动也要产生 mouseMove：悬停指针反馈

    resetView();
}

double StageView::defaultScale() const
{
    // 1:50 按制图惯例是屏幕物理比例：屏幕上 1cm 对应舞台 50cm。
    // 屏幕 1cm 的像素数 = 逻辑 DPI / 2.54，故 scale = dpi / 2.54 / 50（96 DPI 时 ≈ 0.756）。
    const QScreen* s = screen() ? screen() : QGuiApplication::primaryScreen();
    const double dpi = s ? s->logicalDotsPerInch() : 96.0;
    return dpi / 2.54 / kScaleDenominator;
}

void StageView::resetView()
{
    const double scale = defaultScale();
    setTransform(QTransform::fromScale(scale, scale));
    centerOn(m_scene->contentRect().center()); // 视野中心含准备区（阶段10）
}

void StageView::wheelEvent(QWheelEvent* event)
{
    const int steps = event->angleDelta().y();
    if (steps == 0) {
        event->ignore();
        return;
    }
    const double current = transform().m11();
    const double next = qBound(kMinScale, current * std::pow(kZoomFactor, steps / 120.0), kMaxScale);
    if (next != current)
        setTransform(QTransform::fromScale(next, next));
    event->accept();
}

void StageView::mousePressEvent(QMouseEvent* event)
{
    // 手势进行中吞掉其余按下，避免手势嵌套
    if (m_dragMode != DragMode::None) {
        event->accept();
        return;
    }

    if (event->button() == Qt::RightButton) {
        m_panning = true;
        m_panMoved = false;
        m_panPressPos = event->position().toPoint();
        m_lastPanPos = m_panPressPos;
        viewport()->setCursor(Qt::ClosedHandCursor);
        event->accept(); // 不转发给场景；纯点击的菜单由 contextMenuEvent 处理
        return;
    }

    if (event->button() == Qt::LeftButton) {
        setFocus();
        m_pressPos = event->position().toPoint();
        m_pressScenePos = mapToScene(m_pressPos);
        m_pressCtrl = event->modifiers().testFlag(Qt::ControlModifier);
        m_pendingToggleId.clear();

        SelectionModel* selection = m_editorCore->selection();
        StageObjectItem* hit = itemAtViewPos(m_pressPos);

        if (hit && !m_pressCtrl && hit->hitRotationHandle(m_pressScenePos)) {
            // 拖拽朝向手柄：演员白三角 / 屏风前端凸点（Ctrl 按下时优先多选语义）
            m_dragMode = DragMode::Rotating;
            m_rotateId = hit->objectId();
            m_rotateStartRotation = hit->rotation(); // 手势起点显示角度（需求角度制）
            if (!selection->isSelected(m_rotateId))
                selection->select(m_rotateId);
            viewport()->setCursor(Qt::PointingHandCursor); // Rotating 期间保持旋转指针
            event->accept();
            return;
        }

        if (hit) {
            m_dragMode = DragMode::MaybeMove;
            m_primaryId = hit->objectId();
            if (m_pressCtrl) {
                // Ctrl+点已选中对象：先记住，松开未拖动才反选（拖动则保持整体移动）
                if (selection->isSelected(m_primaryId))
                    m_pendingToggleId = m_primaryId;
                else
                    selection->toggle(m_primaryId);
            } else if (!selection->isSelected(m_primaryId)) {
                selection->select(m_primaryId);
            }
            event->accept();
            return;
        }

        // 阶段12：双帧态路径命中（对象之后兜底；路径点优先于白线）
        if (const auto& segment = m_editorCore->activeSegment()) {
            const int pointIndex = pathPointAtViewPos(m_pressPos);
            if (pointIndex >= 0) {
                // 选中路径点 + 待命拖拽（越过阈值转拖动，松开一次性提交）
                m_editorCore->setActivePath(ActivePathRef { *segment, pointIndex });
                m_dragMode = DragMode::MaybePathPoint;
                m_pathPointIndex = pointIndex;
                event->accept();
                return;
            }
            if (pathLineAtViewPos(m_pressPos)) {
                // 点选白线 → activePath（Inspector 切路径页），不进框选
                m_editorCore->setActivePath(ActivePathRef { *segment, -1 });
                event->accept();
                return;
            }
        }

        // 空白处：待判定框选 / 点空白取消选择
        m_dragMode = DragMode::MaybeBand;
        event->accept();
        return;
    }

    QGraphicsView::mousePressEvent(event);
}

void StageView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_panning) {
        const QPoint pos = event->position().toPoint();
        if (!m_panMoved
            && (pos - m_panPressPos).manhattanLength() >= QApplication::startDragDistance()) {
            m_panMoved = true;
        }
        if (m_panMoved) {
            const QPoint delta = pos - m_lastPanPos;
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
            verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        }
        m_lastPanPos = pos;
        event->accept();
        return;
    }

    const QPoint pos = event->position().toPoint();
    switch (m_dragMode) {
    case DragMode::MaybeMove: {
        if ((pos - m_pressPos).manhattanLength() < QApplication::startDragDistance())
            break;
        m_dragMode = DragMode::Moving;
        m_pendingToggleId.clear(); // 拖动成立，取消待反选
        viewport()->setCursor(Qt::SizeAllCursor); // Moving 期间保持移动指针
        // 拖拽起点取图元当前显示位置（游标时刻插值结果），与显示值/数据值分离一致
        m_dragStartPositions.clear();
        for (const QString& id : m_editorCore->selection()->selectedIds()) {
            if (StageObjectItem* item = m_scene->itemById(id))
                m_dragStartPositions.insert(id, item->pos());
        }
        if (!m_dragStartPositions.contains(m_primaryId))
            m_dragMode = DragMode::None; // 兜底：主对象已不存在
        break;
    }
    case DragMode::Moving: {
        // 吸附以主对象落点为准，其余对象保持相对位移
        const QPointF primaryStart = m_dragStartPositions.value(m_primaryId);
        const QPointF rawTarget = primaryStart + (mapToScene(pos) - m_pressScenePos);
        const QPointF delta = snappedPoint(rawTarget) - primaryStart;
        for (auto it = m_dragStartPositions.constBegin(); it != m_dragStartPositions.constEnd(); ++it) {
            if (StageObjectItem* item = m_scene->itemById(it.key()))
                item->setPos(it.value() + delta); // 只动图元，不写数据
        }
        // 阶段13 修复：拖拽预览实时阻挡查询（与提交同一命令路径模拟），
        // 穿入禁区 → 被拖图元标红
        QList<ObjectTransform> previewUpdates;
        for (auto it = m_dragStartPositions.constBegin(); it != m_dragStartPositions.constEnd();
             ++it) {
            if (StageObjectItem* item = m_scene->itemById(it.key())) {
                previewUpdates.append(
                    { it.key(),
                      { item->pos().x(), item->pos().y(), item->rotation() } });
                        // 图元角度即显示值（需求角度制与 Qt 恒等映射）
            }
        }
        const bool blocked = m_editorCore->wouldTransformBlocked(previewUpdates);
        for (auto it = m_dragStartPositions.constBegin(); it != m_dragStartPositions.constEnd();
             ++it) {
            if (StageObjectItem* item = m_scene->itemById(it.key()))
                item->setBlockedPreview(blocked);
        }
        break;
    }
    case DragMode::Rotating: {
        StageObjectItem* item = m_scene->itemById(m_rotateId);
        if (!item) {
            m_dragMode = DragMode::None;
            break;
        }
        const QPointF dir = mapToScene(pos) - item->scenePos();
        if (std::abs(dir.x()) > 1e-9 || std::abs(dir.y()) > 1e-9) {
            // 需求角度制经 RenderUtil 换算一次；15° 吸附实时生效（所见即所得）；
            // 只转图元，松开才提交
            item->setRotation(
                RenderUtil::toQtRotation(snappedRotation(RenderUtil::rotationFromVector(dir))));
        }
        break;
    }
    case DragMode::MaybeBand: {
        if ((pos - m_pressPos).manhattanLength() < QApplication::startDragDistance())
            break;
        m_dragMode = DragMode::Banding;
        if (!m_rubberBand)
            m_rubberBand = new QRubberBand(QRubberBand::Rectangle, viewport());
        m_rubberBand->setGeometry(QRect(m_pressPos, pos).normalized());
        m_rubberBand->show();
        break;
    }
    case DragMode::Banding:
        m_rubberBand->setGeometry(QRect(m_pressPos, pos).normalized());
        break;
    case DragMode::MaybePathPoint: {
        // 阶段12：路径点越过阈值转拖动
        if ((pos - m_pressPos).manhattanLength() < QApplication::startDragDistance())
            break;
        m_dragMode = DragMode::MovingPathPoint;
        viewport()->setCursor(Qt::SizeAllCursor); // 与对象拖拽同款指针
        break;
    }
    case DragMode::MovingPathPoint: {
        // 拖动中只动本地预览（吸附与对象一致），松开才提交命令；
        // 阶段13 任务3：预览实时查询阻挡判定（同一采样实现），穿入禁区标红
        if (!m_editorCore->activeSegment()) {
            m_dragMode = DragMode::None; // 段中途消失（防御）
            m_scene->setPathPointPreview(std::nullopt);
            m_scene->setPathPreviewBlocked(false);
            break;
        }
        const SegmentRef& segment = *m_editorCore->activeSegment();
        const QPointF snapped = snappedPoint(mapToScene(pos));
        m_scene->setPathPointPreview(std::make_pair(m_pathPointIndex, snapped));
        // 预览位置代入路径副本 → 采样折线 → 扫掠阻挡判定
        const FileData& file = m_editorCore->runtimeData()->fileData();
        QPointF fromPos;
        QPointF toPos;
        if (resolveSegmentEndpoints(file, segment.objectId, segment.from, segment.to,
                                    fromPos, toPos)) {
            PathData preview;
            if (const PathData* current = PathGeometry::findPathData(
                    file.paths, segment.objectId, segment.from, segment.to))
                preview = *current;
            else {
                preview.objectId = segment.objectId;
                preview.from = segment.from;
                preview.to = segment.to;
                preview.legAngles = { 0.0 };
            }
            if (m_pathPointIndex >= 0 && m_pathPointIndex < preview.points.size()) {
                preview.points[m_pathPointIndex] =
                    PathPoint { snapped.x(), snapped.y() };
                m_scene->setPathPreviewBlocked(PathGeometry::pathBlocked(
                    file, segment.objectId, segment.from, segment.to,
                    PathGeometry::samplePath(preview, fromPos, toPos)));
            }
        }
        break;
    }
    case DragMode::None:
        break;
    }
    if (m_dragMode != DragMode::None) {
        event->accept();
    } else {
        updateHoverCursor(pos); // 悬停反馈（也覆盖手势异常兜底回到 None 的收尾）
        QGraphicsView::mouseMoveEvent(event);
    }
}

void StageView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton && m_panning) {
        m_panning = false;
        updateHoverCursor(event->position().toPoint()); // 平移结束，按悬停位置恢复
        event->accept(); // m_panMoved 保留给 contextMenuEvent 判定是否弹菜单
        return;
    }

    if (event->button() != Qt::LeftButton || m_dragMode == DragMode::None) {
        QGraphicsView::mouseReleaseEvent(event);
        return;
    }

    const DragMode mode = m_dragMode;
    m_dragMode = DragMode::None;
    SelectionModel* selection = m_editorCore->selection();

    switch (mode) {
    case DragMode::MaybeMove:
        // 纯点击（未拖动）：Ctrl 反选 / 折叠为单选
        if (m_pressCtrl) {
            if (!m_pendingToggleId.isEmpty())
                selection->toggle(m_pendingToggleId);
        } else if (!m_primaryId.isEmpty()) {
            selection->select(m_primaryId);
        }
        m_primaryId.clear();
        break;

    case DragMode::Moving: {
        // 松开一次性提交命令（游标时刻语义：T 处有帧更新、无帧按设置/弹窗决定打帧）；
        // 拖动过程中的图元位移由全量重建覆盖，被拒（选"否"/穿阻挡）时手动回弹
        const QPointF primaryStart = m_dragStartPositions.value(m_primaryId);
        const QPointF rawTarget = primaryStart + (mapToScene(event->position().toPoint()) - m_pressScenePos);
        const QPointF delta = snappedPoint(rawTarget) - primaryStart;
        QList<ObjectTransform> updates;
        for (auto it = m_dragStartPositions.constBegin(); it != m_dragStartPositions.constEnd(); ++it) {
            if (StageObjectItem* item = m_scene->itemById(it.key())) {
                updates.append({ it.key(),
                                 { it.value().x() + delta.x(), it.value().y() + delta.y(),
                                   item->rotation() } }); // 图元角度即显示值（需求角度制）
            }
        }
        if (!m_editorCore->setTransformsAtCursor(updates, keyframeConfirmCallback()))
            m_scene->applyDisplayTransforms(); // 无变更/选"否"/穿阻挡被拒：回弹到插值显示位置
        for (auto it = m_dragStartPositions.constBegin(); it != m_dragStartPositions.constEnd();
             ++it) {
            if (StageObjectItem* item = m_scene->itemById(it.key()))
                item->setBlockedPreview(false); // 阶段13 修复：手势结束清除标红
        }
        m_dragStartPositions.clear();
        m_primaryId.clear();
        break;
    }

    case DragMode::Rotating: {
        StageObjectItem* item = m_scene->itemById(m_rotateId);
        if (item) {
            // 以松开点重新计算（最后一次 mouseMove 的位置未必是松开位置）；
            // 松开点贴近中心（<5cm，含纯点击的映射噪声）时保持手势起点角度，
            // 避免朝向被亚像素偏移带偏；超过阈值按 15° 吸附提交。
            // 基准是手势起点的显示角度（拖动中图元角度被实时改过，不能作基准）
            const QPointF dir = mapToScene(event->position().toPoint()) - item->scenePos();
            double degrees = m_rotateStartRotation;
            if (std::hypot(dir.x(), dir.y()) >= 5.0)
                degrees = snappedRotation(RenderUtil::rotationFromVector(dir));
            bool committed = false;
            if (degrees != m_rotateStartRotation) {
                committed = m_editorCore->setTransformsAtCursor(
                    { { m_rotateId, { item->pos().x(), item->pos().y(), degrees } } },
                    keyframeConfirmCallback());
            }
            if (!committed)
                m_scene->applyDisplayTransforms(); // 无变更/选"否"/原地松开：回弹显示
        }
        m_rotateId.clear();
        break;
    }

    case DragMode::MaybeBand:
        // 点空白取消选择（Ctrl 按下保留现有多选）
        if (!m_pressCtrl)
            selection->clear();
        break;

    case DragMode::Banding: {
        m_rubberBand->hide();
        const QRect viewRect = QRect(m_pressPos, event->position().toPoint()).normalized();
        const QRectF sceneRect = mapToScene(viewRect).boundingRect();
        QList<QString> ids = m_pressCtrl ? selection->selectedIds() : QList<QString>();
        for (StageObjectItem* item : m_scene->stageItems()) {
            // 中心点被框住即选中
            if (sceneRect.contains(item->scenePos()) && !ids.contains(item->objectId()))
                ids.append(item->objectId());
        }
        selection->setSelection(ids);
        break;
    }

    case DragMode::MaybePathPoint:
        break; // 纯点击 = 选中路径点（按下时已写入 activePath.pointIndex）

    case DragMode::MovingPathPoint: {
        // 阶段12：松开一次性提交（以松开点吸附值为准；预览清除，由重建回喂）；
        // 阶段13 任务3：穿入禁区时命令拒绝提交 → 先清预览/标红即回弹原位
        m_scene->setPathPointPreview(std::nullopt);
        m_scene->setPathPreviewBlocked(false);
        if (const auto& segment = m_editorCore->activeSegment()) {
            const QPointF scenePos = snappedPoint(mapToScene(event->position().toPoint()));
            m_editorCore->movePathPoint(segment->objectId, segment->from, segment->to,
                                        m_pathPointIndex, scenePos); // 阻挡则拒绝（数据不变）
        }
        m_pathPointIndex = -1;
        break;
    }

    case DragMode::None:
        break;
    }

    m_pendingToggleId.clear();
    updateHoverCursor(event->position().toPoint()); // 手势结束，按悬停位置恢复
    event->accept();
}

void StageView::contextMenuEvent(QContextMenuEvent* event)
{
    if (m_panMoved) {
        // 右键拖动平移后松开触发的这次：吞掉，不弹菜单
        m_panMoved = false;
        event->accept();
        return;
    }
    showContextMenu(event->pos());
    event->accept();
}

void StageView::keyPressEvent(QKeyEvent* event)
{
    QPointF step;
    switch (event->key()) {
    case Qt::Key_Left:  step = QPointF(-AppSettings::arrowKeyStepCm, 0.0); break;
    case Qt::Key_Right: step = QPointF(AppSettings::arrowKeyStepCm, 0.0);  break;
    case Qt::Key_Up:    step = QPointF(0.0, -AppSettings::arrowKeyStepCm); break;
    case Qt::Key_Down:  step = QPointF(0.0, AppSettings::arrowKeyStepCm);  break;
    default:
        QGraphicsView::keyPressEvent(event);
        return;
    }
    if (m_editorCore->selection()->isEmpty()) {
        // 阶段12：无对象选中但有选中路径点 → 方向键微调路径点（不吸附，步长同对象）
        if (const auto& path = m_editorCore->activePath(); path && path->pointIndex >= 0) {
            const PathData* data = m_editorCore->findPath(
                path->segment.objectId, path->segment.from, path->segment.to);
            if (data && path->pointIndex < data->points.size()) {
                const PathPoint& point = data->points.at(path->pointIndex);
                m_editorCore->movePathPoint(path->segment.objectId, path->segment.from,
                                            path->segment.to, path->pointIndex,
                                            QPointF(point.x + step.x(),
                                                    point.y + step.y()));
            }
            event->accept();
            return;
        }
        QGraphicsView::keyPressEvent(event); // 无选中时保留滚动条默认行为
        return;
    }
    m_editorCore->moveSelectionBy(step, keyframeConfirmCallback()); // 微调不吸附
    event->accept();
}

QPointF StageView::snappedPoint(const QPointF& posCm) const
{
    const AppSettings& settings = m_editorCore->runtimeData()->settings();
    if (!settings.gridSnapEnabled())
        return posCm;
    // 网格间距/吸附距离均为用户可调设置，拖拽处即时读取
    const double grid = settings.gridSizeCm();
    const double range = settings.snapRangeCm();
    QPointF result = posCm;
    const double snappedX = std::round(posCm.x() / grid) * grid;
    if (std::abs(snappedX - posCm.x()) <= range)
        result.setX(snappedX);
    const double snappedY = std::round(posCm.y() / grid) * grid;
    if (std::abs(snappedY - posCm.y()) <= range)
        result.setY(snappedY);
    return result;
}

StageObjectItem* StageView::itemAtViewPos(const QPoint& viewPos) const
{
    return m_scene->topStageItemAt(mapToScene(viewPos));
}

// 阶段12：路径点命中（屏像素 ≤7px）；标记仅在选中路径时存在（场景侧保证）
int StageView::pathPointAtViewPos(const QPoint& viewPos) const
{
    const QList<QPointF> positions = m_scene->pathPointMarkerPositions();
    for (int i = 0; i < positions.size(); ++i) {
        if (QLineF(QPointF(viewPos), mapFromScene(positions.at(i))).length() <= 7.0)
            return i;
    }
    return -1;
}

// 阶段12：白线命中（屏像素 ≤5px）：点到折线的最近距离
bool StageView::pathLineAtViewPos(const QPoint& viewPos) const
{
    const QList<QPointF> polyline = m_scene->segmentPathPolyline();
    if (polyline.size() < 2)
        return false;
    const QPointF p(viewPos);
    for (int i = 0; i + 1 < polyline.size(); ++i) {
        const QPointF a = mapFromScene(polyline.at(i));
        const QPointF ab = mapFromScene(polyline.at(i + 1)) - a;
        const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
        double f = 0.0;
        if (len2 > 0.0) {
            f = qBound(0.0,
                       ((p.x() - a.x()) * ab.x() + (p.y() - a.y()) * ab.y()) / len2, 1.0);
        }
        const QPointF proj = a + ab * f;
        if (std::hypot(p.x() - proj.x(), p.y() - proj.y()) <= 5.0)
            return true;
    }
    return false;
}

std::function<bool()> StageView::keyframeConfirmCallback() const
{
    // 业务层只在"未抑制且 T 处无关键帧"时调用；弹窗属渲染层（含"不再提示"勾选）
    return [this] {
        return KeyframePrompt::confirmAtCursor(
            const_cast<StageView*>(this), m_editorCore->runtimeData()->settings(),
            m_editorCore->timelinePlayer()->currentTime());
    };
}

void StageView::updateHoverCursor(const QPoint& viewPos)
{    // 手形表示"可抓握拖动的操控点"（旋转手柄）；四向箭头是移动的标准语义，
    // 且不与右键平移的握拳手形混淆；空白处 unsetCursor 恢复默认箭头
    if (StageObjectItem* hit = itemAtViewPos(viewPos)) {
        viewport()->setCursor(hit->hitRotationHandle(mapToScene(viewPos))
                                  ? Qt::PointingHandCursor
                                  : Qt::SizeAllCursor);
    } else {
        viewport()->unsetCursor();
    }
}

void StageView::showContextMenu(const QPoint& viewPos)
{
    const QPointF scenePos = mapToScene(viewPos);
    SelectionModel* selection = m_editorCore->selection();

    // 阶段12：双帧态路径右键（对象优先、路径点优先于白线）——
    // 路径点上：删除路径点；白线上：添加路径点 / 恢复直线
    if (const auto& segment = m_editorCore->activeSegment(); segment && !itemAtViewPos(viewPos)) {
        const int pointIndex = pathPointAtViewPos(viewPos);
        if (pointIndex >= 0) {
            m_editorCore->setActivePath(ActivePathRef { *segment, pointIndex });
            QMenu menu(this);
            QAction* removePointAction = menu.addAction(QStringLiteral("删除路径点"));
            if (menu.exec(viewport()->mapToGlobal(viewPos)) == removePointAction) {
                m_editorCore->removePathPoint(segment->objectId, segment->from, segment->to,
                                              pointIndex);
            }
            return;
        }
        if (pathLineAtViewPos(viewPos)) {
            m_editorCore->setActivePath(ActivePathRef { *segment, -1 }); // 右键白线先选中路径
            QMenu menu(this);
            QAction* addPointAction = menu.addAction(QStringLiteral("添加路径点"));
            QAction* resetAction = menu.addAction(QStringLiteral("恢复直线"));
            // 零存储默认态：无 PathData（已是直线）时"恢复直线"禁用
            resetAction->setEnabled(
                m_editorCore->findPath(segment->objectId, segment->from, segment->to)
                != nullptr);
            QAction* chosen = menu.exec(viewport()->mapToGlobal(viewPos));
            if (chosen == addPointAction) {
                // 点吸附落在曲线上（命令内部投影），圆心角保形拆分
                m_editorCore->addPathPoint(segment->objectId, segment->from, segment->to,
                                           scenePos);
            } else if (chosen == resetAction) {
                m_editorCore->resetPath(segment->objectId, segment->from, segment->to);
            }
            return;
        }
    }

    // 右键落在未选中对象上：先把它单选，菜单操作即作用于它
    if (StageObjectItem* hit = itemAtViewPos(viewPos)) {
        if (!selection->isSelected(hit->objectId()))
            selection->select(hit->objectId());
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

    QAction* chosen = menu.exec(viewport()->mapToGlobal(viewPos));
    if (!chosen)
        return;

    if (chosen == createAction) {
        CreateObjectDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted)
            m_editorCore->createObject(dialog.selectedType(), scenePos); // 落在右键位置
    } else if (chosen == copyAction) {
        m_editorCore->copySelection();
    } else if (chosen == pasteAction) {
        m_editorCore->pasteClipboard(scenePos); // 落在鼠标右键处
    } else if (chosen == deleteAction) {
        m_editorCore->deleteSelection();
    }
}
