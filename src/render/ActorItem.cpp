#include "ActorItem.h"

#include "RenderUtil.h"
#include "data/AppSettings.h"

#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>

ActorItem::ActorItem(const ObjectData& data, QGraphicsItem* parent)
    : StageObjectItem(data, parent)
    , m_color(data.color)
    , m_radius(AppSettings::actorRadiusCm)
{
    setPos(data.transform.x, data.transform.y);
    setRotation(RenderUtil::toQtRotation(data.transform.rotation));
}

QPolygonF ActorItem::trianglePolygon() const
{
    // 局部坐标指向 +y（下），朝向由 setRotation 表达
    // 三角在圆形外侧：底边贴圆周外（留 0.1r 缝隙）、尖端朝外指向朝向
    return QPolygonF {
        QPointF(0.0, m_radius * 1.6),
        QPointF(-m_radius * 0.25, m_radius * 1.1),
        QPointF(m_radius * 0.25, m_radius * 1.1),
    };
}

QRectF ActorItem::boundingRect() const
{
    // 覆盖圆 + 圆外三角（下边界到尖端外 2cm 余量）
    return QRectF(-m_radius - 2.0, -m_radius - 2.0,
                  2.0 * m_radius + 4.0, m_radius * 2.6 + 4.0);
}

bool ActorItem::hitRotationHandle(const QPointF& scenePos) const
{
    const QPointF local = mapFromScene(scenePos);
    QPainterPath path;
    path.addPolygon(trianglePolygon());
    path.closeSubpath();
    if (path.contains(local))
        return true;
    // 容差描边：白三角在默认 1:50 比例下较小，外扩 5cm 便于点中
    QPainterPathStroker stroker;
    stroker.setWidth(5.0);
    return stroker.createStroke(path).contains(local);
}

void ActorItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/, QWidget* /*widget*/)
{
    // 圆（圆心为定位点），描边 1px 不随缩放变化
    QPen pen(QColor(QStringLiteral("#202020")));
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->setBrush(m_color.isValid() ? m_color : QColor(QStringLiteral("#e74c3c")));
    painter->drawEllipse(QPointF(0.0, 0.0), m_radius, m_radius);

    // 白色朝向三角：在圆形外侧（底边贴圆周外、尖端朝外），局部坐标指向 +y
    painter->setPen(Qt::NoPen);
    painter->setBrush(Qt::white);
    painter->drawPolygon(trianglePolygon());

    // 选中高亮：加粗实线圈
    QPainterPath circle;
    circle.addEllipse(QPointF(0.0, 0.0), m_radius, m_radius);
    drawHighlight(painter, circle);
}
