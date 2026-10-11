#include "StageObjectItem.h"

#include <QPainter>
#include <QPainterPath>

StageObjectItem::StageObjectItem(const ObjectData& data, QGraphicsItem* parent)
    : QGraphicsItem(parent)
    , m_id(data.id)
{
    // 交互手势由 StageView 统一实现（框选需中心点判定、拖动需整体位移），
    // 图元本身不接收鼠标按键，命中测试走场景空间查询。
    setAcceptedMouseButtons(Qt::NoButton);
}

void StageObjectItem::setHighlighted(bool highlighted)
{
    if (m_highlighted == highlighted)
        return;
    m_highlighted = highlighted;
    update();
}

void StageObjectItem::drawHighlight(QPainter* painter, const QPainterPath& shape) const
{
    if (!m_highlighted)
        return;
    // 选中高亮：实线加粗描边（cosmetic 笔恒 4px，不随缩放变细）；
    // 先垫一圈深色底再压亮蓝主线，保证在深色画布与彩色对象本体上都醒目
    QPen underPen(QColor(QStringLiteral("#1a1a1a")));
    underPen.setWidth(6);
    underPen.setCosmetic(true);
    painter->setPen(underPen);
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(shape);

    QPen pen(QColor(QStringLiteral("#4a90d9"))); // 亮蓝实线高亮
    pen.setWidth(4);
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->drawPath(shape);
}
