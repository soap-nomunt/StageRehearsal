#pragma once

// 设置页"舞台与准备区"方块编辑器（阶段10）：Windows 显示设置式的俯视方块布置图。
// 数据进/信号出的哑控件，不持业务层指针：内容经 setContent 由 SettingsWindow
// 喂入（fileDataChanged 时），交互结果以信号抛出、由 SettingsWindow 转调
// EditorCore（提交链路：信号 → EditorCore add/remove/setPrepAreas →
// commitFileData → fileDataChanged → 回喂画布）。
// 全部方块包围盒自适应缩放留边；舞台方块固定为参照物不可拖（y 由坐标系定义
// 锁定，位置由"中心点偏移"数值行调整）；准备区方块左键可拖——拖动中实时移动
// 本地预览（50cm 网格吸附），松开才发一次移动请求。
// 右键平移（同主视口 StageView 语义）：任意位置（空白/方块上一视同仁）右键按住
// 即进入平移手势（握拳光标），越过拖拽阈值后内容随手移动——像素偏移叠加在
// 自适应适配变换之上（updateTransform 重算不吞偏移），无滚动条故钳制限界：
// 内容包围盒始终与控件保持相交，不会把内容推出视野。平移过则吞掉随之而来的
// contextMenuEvent（Windows 右键松开即发 WM_CONTEXTMENU），不弹菜单；
// 纯右键点击（未越阈值松开）照旧：空白 = 新建准备区（右键处吸附坐标）、
// 准备区 = 删除、舞台 = 无菜单。
// 选中态为画布局部状态：恰好一个方块被选中（默认舞台），随 setContent 校验
// 存在性（准备区被删后回落选中舞台并发 selectionChanged）。

#include "data/DataTypes.h"

#include <QList>
#include <QRectF>
#include <QString>
#include <QWidget>

class AppSettings;
class QContextMenuEvent;
class QMouseEvent;

class PrepAreaCanvas : public QWidget {
    Q_OBJECT
public:
    explicit PrepAreaCanvas(const AppSettings* settings, QWidget* parent = nullptr);

    static constexpr double kSnapGridCm = 50.0; // 拖拽/新建的网格吸附步长
    static QString stageBlockId() { return QStringLiteral("stage"); } // 舞台方块固定 id

    void setContent(const QRectF& stageRect, const QList<PrepAreaData>& prepAreas);

    // 选中：恰好一个方块，返回 stageBlockId() 或准备区 id
    QString selectedBlockId() const { return m_selectedId; }
    void setSelectedId(const QString& id); // 不存在的 id 忽略

    // 自测钩子：方块在画布上的像素矩形（不存在返回空矩形），供 QTest 合成事件定位
    QRect blockRectFor(const QString& id) const;

signals:
    void prepAreaMoveRequested(const QString& id, double x, double y); // 松开时一次，坐标已吸附
    void prepAreaAddRequested(double x, double y);                     // 右键空白新建，坐标已吸附
    void prepAreaRemoveRequested(const QString& id);
    void selectionChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    void updateTransform(); // 内容包围盒（舞台 ∪ 准备区）→ 画布像素的自适应缩放变换
    void clampPanOffset();  // 平移限界：内容包围盒始终与控件保持相交（画布无滚动条）
    void drawBlock(QPainter* painter, const QString& id, const QColor& fill,
                   const QString& name, double widthCm, double heightCm) const;
    QRectF blockSceneRect(const QString& id) const;  // 场景矩形（cm；拖动中取预览位置）
    QString blockAt(const QPoint& canvasPos) const;  // 命中最上层方块 id，空白返回空串
    QPointF sceneFromCanvas(const QPointF& canvasPos) const;
    QRectF canvasFromScene(const QRectF& sceneRect) const;
    static QPointF snappedToGrid(const QPointF& posCm);

    const AppSettings* m_settings = nullptr; // 不拥有（舞台色/派生色取色源）

    QRectF m_stageRect;
    QList<PrepAreaData> m_prepAreas;
    QString m_selectedId; // 默认 stageBlockId()

    QRectF m_contentBBox;   // 内容包围盒（场景坐标，updateTransform 缓存，限界用）
    double m_scale = 1.0;   // cm → 画布像素
    QPointF m_origin;       // 场景原点在画布上的像素位置（自适应适配结果）
    QPointF m_panOffset;    // 右键平移像素偏移（叠加在适配之上；setContent/resize 不重置）

    // 右键平移（同主视口 StageView）：按下即进入手势，越阈值才移动；
    // m_panMoved 保留给 contextMenuEvent 判定是否弹菜单（平移过 = 吞掉不弹）
    bool m_panning = false;
    bool m_panMoved = false;
    QPoint m_panPressPos;
    QPoint m_lastPanPos;

    // 左键拖拽预览：只动本地预览不写数据，松开才发一次 prepAreaMoveRequested
    QString m_dragId;          // 非空 = 左键按在准备区上（待判定/拖动中）
    bool m_dragging = false;   // 已越过拖动阈值
    QPoint m_pressPos;         // 按下点（画布像素）
    QPointF m_grabOffset;      // 按下时方块中心 − 按下点（场景坐标，保持抓取偏移）
    QPointF m_previewCenter;   // 吸附后的预览中心（场景坐标）
};
