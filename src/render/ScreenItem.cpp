#include "ScreenItem.h"

#include "RenderUtil.h"
#include "data/AppSettings.h"

#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>

ScreenItem::ScreenItem(const ObjectData& data, QGraphicsItem* parent)
    : StageObjectItem(data, parent)
    , m_color(data.color)
    , m_size(data.size.value_or(QSizeF(AppSettings::screenDefaultWidthCm, AppSettings::screenDefaultHeightCm)))
{
    setPos(data.transform.x, data.transform.y);
    setRotation(RenderUtil::toQtRotation(data.transform.rotation));
}

QPolygonF ScreenItem::handlePolygon() const
{
    // 朝向凸点：矩形前端（局部 +y）中点外凸的小三角
    const double front = m_size.height() / 2.0;
    return QPolygonF {
        QPointF(0.0, front + 16.0),
        QPointF(-9.0, front + 1.0),
        QPointF(9.0, front + 1.0),
    };
}

QRectF ScreenItem::boundingRect() const
{
    const double w = m_size.width();
    const double h = m_size.height();
    return QRectF(-w / 2.0 - 2.0, -h / 2.0 - 2.0, w + 4.0, h + 4.0 + 18.0); // 含朝向凸点
}

bool ScreenItem::hitRotationHandle(const QPointF& scenePos) const
{
    const QPointF local = mapFromScene(scenePos);
    QPainterPath path;
    path.addPolygon(handlePolygon());
    path.closeSubpath();
    if (path.contains(local))
        return true;
    // 容差描边：凸点较小，外扩 6cm 便于点中
    QPainterPathStroker stroker;
    stroker.setWidth(6.0);
    return stroker.createStroke(path).contains(local);
}

void ScreenItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/)
{
    QPen pen(QColor(QStringLiteral("#202020")));
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->setBrush(m_color.isValid() ? m_color : QColor(QStringLiteral("#888888")));
    painter->drawRect(QRectF(-m_size.width() / 2.0, -m_size.height() / 2.0,
                             m_size.width(), m_size.height()));

    // 朝向凸点：白色填充 + 深色描边（白色直接画在舞台上不可见）
    QPen handlePen(QColor(QStringLiteral("#202020")));
    handlePen.setCosmetic(true);
    painter->setPen(handlePen);
    painter->setBrush(Qt::white);
    painter->drawPolygon(handlePolygon());

    // 选中高亮：加粗实线框
    QPainterPath rect;
    rect.addRect(QRectF(-m_size.width() / 2.0, -m_size.height() / 2.0,
                        m_size.width(), m_size.height()));
    drawHighlight(painter, rect);
}
