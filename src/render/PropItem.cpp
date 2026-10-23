#include "PropItem.h"

#include "RenderUtil.h"
#include "business/PathGeometry.h"

#include <QPainter>

PropItem::PropItem(const ObjectData& data, QGraphicsItem* parent)
    : StageObjectItem(data, parent)
    , m_color(data.color)
{
    setPos(data.transform.x, data.transform.y);
    setRotation(RenderUtil::toQtRotation(data.transform.rotation));

    // 实际几何 = 顶点 × scale（局部坐标）；逐边按圆心角采样折线化
    // （与路径编辑/阻挡判定同一采样实现）
    const PropShape shape = data.shape.value_or(defaultPropShape());
    const int count = shape.vertices.size();
    for (int i = 0; i < count; ++i) {
        const PathPoint& va = shape.vertices.at(i);
        const PathPoint& vb = shape.vertices.at((i + 1) % count);
        const QPointF a(va.x * data.scaleX, va.y * data.scaleY);
        const QPointF b(vb.x * data.scaleX, vb.y * data.scaleY);
        const double angle = i < shape.edgeAngles.size() ? shape.edgeAngles.at(i) : 0.0;
        const QList<QPointF> leg = PathGeometry::sampleLeg(a, b, angle);
        for (int j = (i == 0 ? 0 : 1); j < leg.size(); ++j) { // 段间连接点不重复
            if (m_outline.elementCount() == 0)
                m_outline.moveTo(leg.at(j));
            else
                m_outline.lineTo(leg.at(j));
        }
    }
    m_outline.closeSubpath();
}

QRectF PropItem::boundingRect() const
{
    // 轮廓包围盒 + 高亮描边余量（同演员/屏风先例）
    return m_outline.boundingRect().adjusted(-2.0, -2.0, 2.0, 2.0);
}

QPainterPath PropItem::shape() const
{
    return m_outline; // 命中 = 点在多边形内
}

void PropItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/,
                     QWidget* /*widget*/)
{
    // 填充 + 1px 深色描边（不随缩放变化）；道具不画朝向标记
    QPen pen(QColor(QStringLiteral("#202020")));
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->setBrush(m_color.isValid() ? m_color : QColor(QStringLiteral("#b0814e")));
    painter->drawPath(m_outline);

    // 选中高亮：加粗实线描边
    drawHighlight(painter, m_outline);
    drawBlockedOverlay(painter, m_outline); // 阶段13：阻挡预览标红
}

bool PropItem::hitRotationHandle(const QPointF& /*scenePos*/) const
{
    return false; // 道具无朝向手柄
}
