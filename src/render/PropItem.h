#pragma once

// 舞台道具图元（阶段13）：多边形 + 弧边形状（局部顶点 × scale 后逐边采样，
// 复用 PathGeometry 同一实现——渲染/阻挡/形状编辑器同源），缺省 shape =
// 默认 100×100cm 矩形；无朝向标记（决策：道具不画朝向手柄，朝向仅经
// Inspector 数值修改）；选中高亮沿用基座描边。命中 = 点在多边形内
// （shape() 覆盖）；框选沿用中心点规则；双帧幽灵复用基座 m_ghost 机制。
// 鼠标手势由 StageView 统一实现。

#include "StageObjectItem.h"

#include <QPainterPath>

class PropItem : public StageObjectItem {
public:
    explicit PropItem(const ObjectData& data, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    // 命中判定用轮廓（采样后多边形）：点在多边形内即命中
    QPainterPath shape() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option,
               QWidget* widget = nullptr) override;

    // 道具无朝向手柄：恒 false
    bool hitRotationHandle(const QPointF& scenePos) const override;

private:
    QPainterPath m_outline; // 局部坐标已采样轮廓（顶点 × scale、弧边已折线化）
    QColor m_color;
};
