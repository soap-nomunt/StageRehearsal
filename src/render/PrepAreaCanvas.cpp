#include "PrepAreaCanvas.h"

#include "data/AppSettings.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>

#include <cmath>

namespace {

constexpr double kMarginPx = 24.0; // 包围盒自适应缩放后的四周留边（像素）

} // namespace

PrepAreaCanvas::PrepAreaCanvas(const AppSettings* settings, QWidget* parent)
    : QWidget(parent)
    , m_settings(settings)
    , m_selectedId(stageBlockId())
{
    setObjectName(QStringLiteral("prepAreaCanvas"));
    setMinimumHeight(280); // 设置页内留足可操作高度
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(false);
}

void PrepAreaCanvas::setContent(const QRectF& stageRect, const QList<PrepAreaData>& prepAreas)
{
    m_stageRect = stageRect;
    m_prepAreas = prepAreas;
    // 选中的准备区被删（removePrepArea）后回落选中舞台
    if (m_selectedId != stageBlockId()) {
        bool exists = false;
        for (const PrepAreaData& area : m_prepAreas) {
            if (area.id == m_selectedId) {
                exists = true;
                break;
            }
        }
        if (!exists) {
            m_selectedId = stageBlockId();
            emit selectionChanged();
        }
    }
    updateTransform();
    update();
}

void PrepAreaCanvas::setSelectedId(const QString& id)
{
    if (id == m_selectedId)
        return;
    if (id != stageBlockId()) {
        bool exists = false;
        for (const PrepAreaData& area : m_prepAreas) {
            if (area.id == id) {
                exists = true;
                break;
            }
        }
        if (!exists)
            return;
    }
    m_selectedId = id;
    emit selectionChanged();
    update();
}

QRect PrepAreaCanvas::blockRectFor(const QString& id) const
{
    const QRectF sceneRect = blockSceneRect(id);
    return sceneRect.isNull() ? QRect() : canvasFromScene(sceneRect).toRect();
}

void PrepAreaCanvas::updateTransform()
{
    QRectF bbox = m_stageRect;
    for (const PrepAreaData& area : m_prepAreas) {
        bbox = bbox.united(QRectF(area.x - area.width / 2.0, area.y - area.height / 2.0,
                                  area.width, area.height));
    }
    if (bbox.isEmpty()) // 防御：舞台宽高恒 > 0，正常不会走到
        bbox = QRectF(-1.0, -1.0, 2.0, 2.0);

    const double availW = qMax(1.0, width() - 2.0 * kMarginPx);
    const double availH = qMax(1.0, height() - 2.0 * kMarginPx);
    m_scale = qMin(availW / bbox.width(), availH / bbox.height());
    // 包围盒在画布内居中
    m_origin = QPointF((width() - bbox.width() * m_scale) / 2.0 - bbox.left() * m_scale,
                       (height() - bbox.height() * m_scale) / 2.0 - bbox.top() * m_scale);
}

QRectF PrepAreaCanvas::blockSceneRect(const QString& id) const
{
    if (id == stageBlockId())
        return m_stageRect;
    for (const PrepAreaData& area : m_prepAreas) {
        if (area.id == id) {
            // 拖动中的方块画在本地预览位置（松开提交后由 setContent 回喂正式值）
            const QPointF center = (m_dragging && id == m_dragId)
                ? m_previewCenter
                : QPointF(area.x, area.y);
            return QRectF(center.x() - area.width / 2.0, center.y() - area.height / 2.0,
                          area.width, area.height);
        }
    }
    return QRectF();
}

QString PrepAreaCanvas::blockAt(const QPoint& canvasPos) const
{
    // 后画的在上层：准备区逆序（后添加的优先命中），舞台恒在底层
    for (auto it = m_prepAreas.crbegin(); it != m_prepAreas.crend(); ++it) {
        if (canvasFromScene(blockSceneRect(it->id)).contains(canvasPos))
            return it->id;
    }
    if (canvasFromScene(m_stageRect).contains(canvasPos))
        return stageBlockId();
    return QString();
}

QPointF PrepAreaCanvas::sceneFromCanvas(const QPointF& canvasPos) const
{
    return QPointF((canvasPos.x() - m_origin.x()) / m_scale,
                   (canvasPos.y() - m_origin.y()) / m_scale);
}

QRectF PrepAreaCanvas::canvasFromScene(const QRectF& sceneRect) const
{
    return QRectF(m_origin.x() + sceneRect.left() * m_scale,
                  m_origin.y() + sceneRect.top() * m_scale,
                  sceneRect.width() * m_scale, sceneRect.height() * m_scale);
}

QPointF PrepAreaCanvas::snappedToGrid(const QPointF& posCm)
{
    return QPointF(std::round(posCm.x() / kSnapGridCm) * kSnapGridCm,
                   std::round(posCm.y() / kSnapGridCm) * kSnapGridCm);
}

void PrepAreaCanvas::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(QStringLiteral("#242424"))); // 与主视口同底色

    const QColor stageColor =
        m_settings ? m_settings->stageColor() : AppSettings::defaultStageColor();
    const QColor prepColor = stageColor.darker(160); // 准备区颜色固定从舞台色派生

    // 舞台在底层，准备区按列表顺序叠画
    drawBlock(&painter, stageBlockId(), stageColor, QStringLiteral("舞台"),
              m_stageRect.width(), m_stageRect.height());
    for (const PrepAreaData& area : m_prepAreas)
        drawBlock(&painter, area.id, prepColor, area.name, area.width, area.height);
}

void PrepAreaCanvas::drawBlock(QPainter* painter, const QString& id, const QColor& fill,
                               const QString& name, double widthCm, double heightCm) const
{
    const QRectF r = canvasFromScene(blockSceneRect(id));
    if (r.isEmpty())
        return;

    painter->setPen(Qt::NoPen);
    painter->setBrush(fill);
    painter->drawRect(r);

    // 选中高亮：与主视口同一套亮蓝描边（先垫深色底保证醒目）
    if (id == m_selectedId) {
        QPen underPen(QColor(QStringLiteral("#1a1a1a")));
        underPen.setWidth(5);
        painter->setPen(underPen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(r);
        QPen pen(QColor(QStringLiteral("#4a90d9")));
        pen.setWidth(3);
        painter->setPen(pen);
        painter->drawRect(r);
    } else {
        QPen pen(QColor(QStringLiteral("#333333")));
        pen.setWidth(1);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawRect(r);
    }

    // 方块内居中两行文字（仿 Windows 显示设置）：名称 + "宽 × 深"
    painter->setPen(QColor(QStringLiteral("#f0f0f0")));
    const QString dims = QStringLiteral("%1 × %2 cm")
                             .arg(QString::number(widthCm), QString::number(heightCm));
    painter->drawText(r, Qt::AlignCenter, name + QLatin1Char('\n') + dims);
}

void PrepAreaCanvas::resizeEvent(QResizeEvent* event)
{
    updateTransform();
    QWidget::resizeEvent(event);
}

void PrepAreaCanvas::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        const QString hit = blockAt(event->pos());
        if (!hit.isEmpty()) {
            if (hit != m_selectedId) {
                m_selectedId = hit;
                emit selectionChanged();
                update();
            }
            if (hit != stageBlockId()) {
                // 准备区可拖：记录按下点与抓取偏移，越过阈值后进入拖动
                m_dragId = hit;
                m_dragging = false;
                m_pressPos = event->pos();
                const QRectF sceneRect = blockSceneRect(hit);
                m_grabOffset = sceneRect.center() - sceneFromCanvas(event->pos());
                m_previewCenter = sceneRect.center();
            }
        }
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void PrepAreaCanvas::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_dragId.isEmpty()) {
        if (!m_dragging
            && (event->pos() - m_pressPos).manhattanLength()
                >= QApplication::startDragDistance()) {
            m_dragging = true;
        }
        if (m_dragging) {
            // 实时本地预览（50cm 网格吸附）；松开才发一次提交
            m_previewCenter = snappedToGrid(sceneFromCanvas(event->pos()) + m_grabOffset);
            update();
        }
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void PrepAreaCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !m_dragId.isEmpty()) {
        const QString id = m_dragId;
        const bool moved = m_dragging;
        m_dragId.clear();
        m_dragging = false;
        if (moved) {
            // 松开才发一次移动请求（吸附后坐标）；预览保持到 setContent 回喂
            emit prepAreaMoveRequested(id, m_previewCenter.x(), m_previewCenter.y());
        }
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void PrepAreaCanvas::contextMenuEvent(QContextMenuEvent* event)
{
    const QString hit = blockAt(event->pos());
    if (hit == stageBlockId())
        return; // 右键舞台：无菜单（舞台唯一，不可新建/删除）

    if (hit.isEmpty()) {
        QMenu menu(this);
        QAction* addAction = menu.addAction(QStringLiteral("新建准备区"));
        if (menu.exec(event->globalPos()) == addAction) {
            const QPointF scenePos = snappedToGrid(sceneFromCanvas(event->pos()));
            emit prepAreaAddRequested(scenePos.x(), scenePos.y());
        }
    } else {
        QMenu menu(this);
        QAction* removeAction = menu.addAction(QStringLiteral("删除"));
        if (menu.exec(event->globalPos()) == removeAction)
            emit prepAreaRemoveRequested(hit);
    }
}
