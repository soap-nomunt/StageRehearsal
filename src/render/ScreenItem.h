#pragma once

// 屏风图元：以定位点为中心的矩形，尺寸取 ObjectData::size，
// 缺失时用 AppSettings 屏风默认 100×10；颜色取 ObjectData::color。
// 阶段3：矩形前端中点（局部 +y）带朝向凸点手柄；支持选中高亮。
// 鼠标手势由 StageView 统一实现。

#include "StageObjectItem.h"

class ScreenItem : public StageObjectItem {
public:
    explicit ScreenItem(const ObjectData& data, QGraphicsItem* parent = nullptr);

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget = nullptr) override;

    // 前端中点朝向凸点（局部坐标多边形 + 容差描边）
    bool hitRotationHandle(const QPointF& scenePos) const override;

private:
    QPolygonF handlePolygon() const; // 局部坐标，与 paint 中绘制的一致

    QColor m_color;
    QSizeF m_size;
};
