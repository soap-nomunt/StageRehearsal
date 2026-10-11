#pragma once

// 演员/屏风图元公共基座：对象 id、选中高亮标记、朝向手柄命中测试。
// 鼠标事件仍由 StageView 统一处理（图元不接收按键），本类只承载状态与绘制。

#include "data/DataTypes.h"

#include <QGraphicsItem>
#include <QString>

class QPainter;
class QPainterPath;

class StageObjectItem : public QGraphicsItem {
public:
    explicit StageObjectItem(const ObjectData& data, QGraphicsItem* parent = nullptr);

    QString objectId() const { return m_id; }

    // 选中高亮（加粗实线描边，paint 中按此标记绘制）；由场景按 SelectionModel 恢复
    void setHighlighted(bool highlighted);
    bool isHighlighted() const { return m_highlighted; }

    // 朝向手柄（演员白三角 / 屏风前端凸点）命中测试，scenePos 为场景坐标
    virtual bool hitRotationHandle(const QPointF& scenePos) const = 0;

protected:
    // 加粗实线高亮描边（深色垫底 + 亮蓝主线，cosmetic 笔不随缩放变粗变细）
    void drawHighlight(QPainter* painter, const QPainterPath& shape) const;

    QString m_id;
    bool m_highlighted = false;
};
