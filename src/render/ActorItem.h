#pragma once

// 演员图元：半径 AppSettings::actorRadiusCm 的圆 + 圆外指向局部 +y 的白色朝向三角。
// 圆心为定位点；朝向由 setRotation 表达（入口处经 RenderUtil 换算一次）。
// 阶段3：支持选中高亮与白三角朝向手柄命中测试；鼠标手势由 StageView 统一实现。
// 阶段3.6：白三角移到圆形外侧（底边贴圆周外、尖端朝外），命中/旋转语义不变。

#include "StageObjectItem.h"

class ActorItem : public StageObjectItem {
public:
    explicit ActorItem(const ObjectData& data, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

    // 白三角朝向手柄（局部坐标多边形 + 容差描边）
    bool hitRotationHandle(const QPointF& scenePos) const override;

private:
    QPolygonF trianglePolygon() const; // 局部坐标，与 paint 中绘制的一致

    QColor m_color;
    double m_radius;
};
