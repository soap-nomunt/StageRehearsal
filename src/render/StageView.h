#pragma once

// 主视口：滚轮缩放（以鼠标为中心）、右键拖动平移（拖动后松开不弹右键菜单）、
// 默认视图比例 1:50（屏幕物理比例：屏幕上 1cm = 舞台 50cm，按屏幕 DPI 换算）。
// 不做相机旋转。
// 阶段3：左键编辑手势统一在此实现（图元不接收鼠标按键）——
// 单击选中 / Ctrl 多选 / 空白框选（中心点判定，自绘橡皮筋）/ 拖拽移动（网格吸附）/
// 朝向手柄旋转 / 方向键微调；纯右键点击（未拖动）弹 Create/复制/粘贴/删除菜单。
// 指针反馈：悬停朝向手柄 → 旋转意图（手形），悬停对象本体 → 移动意图（四向箭头），
// 空白 → 默认箭头；Moving/Rotating 拖拽期间保持对应指针，手势结束按悬停位置恢复。

#include <QGraphicsView>
#include <QHash>
#include <QPoint>
#include <QPointF>

#include <functional>

class EditorCore;
class QRubberBand;
class StageObjectItem;
class StageScene;

class StageView : public QGraphicsView
{
    Q_OBJECT

public:
    explicit StageView(StageScene* scene, EditorCore* editorCore, QWidget* parent = nullptr);

    // 恢复默认比例 1:50，并将视野中心对准舞台中心
    void resetView();

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    enum class DragMode {
        None,      // 无手势
        MaybeMove, // 按下在对象上，待判定位移
        Moving,    // 拖拽移动中（只动图元不写数据，松开一次性提交命令）
        Rotating,  // 拖拽朝向手柄中
        MaybeBand, // 按下在空白处，待判定框选
        Banding,   // 框选中（自绘橡皮筋，中心点判定）
    };

    double defaultScale() const; // 1:50 换算结果（随屏幕 DPI）

    // 网格吸附：开启时吸附到 100cm 网格点、范围 20cm 内生效（逐轴判定）
    QPointF snappedPoint(const QPointF& posCm) const;

    StageObjectItem* itemAtViewPos(const QPoint& viewPos) const;
    // 悬停指针反馈：按 viewPos 命中结果设置指针（手柄 → 旋转意图，本体 → 移动意图，空白 → 箭头）
    void updateHoverCursor(const QPoint& viewPos);
    void showContextMenu(const QPoint& viewPos);
    // 打帧询问弹窗回调（需求第 20 条）：业务层只在需要询问时调用
    std::function<bool()> keyframeConfirmCallback() const;

    static constexpr double kScaleDenominator = 50.0; // 1:50
    static constexpr double kMinScale = 0.05;
    static constexpr double kMaxScale = 4.0;
    static constexpr double kZoomFactor = 1.2; // 每格滚轮

    StageScene* m_scene = nullptr;       // 不拥有
    EditorCore* m_editorCore = nullptr;  // 不拥有

    // 右键平移
    bool m_panning = false;              // 右键按住中
    bool m_panMoved = false;             // 已超过拖动阈值（区分纯右键点击）
    QPoint m_panPressPos;
    QPoint m_lastPanPos;

    // 左键手势
    DragMode m_dragMode = DragMode::None;
    QPoint m_pressPos;                   // 按下点（视图坐标）
    QPointF m_pressScenePos;             // 按下点（场景坐标，cm）
    bool m_pressCtrl = false;            // 按下时 Ctrl 状态
    QString m_pendingToggleId;           // Ctrl 按下已选中对象：松开未拖动才反选
    QString m_primaryId;                 // 拖拽主对象（吸附以其落点为准）
    QHash<QString, QPointF> m_dragStartPositions; // 拖拽起点位置（id → cm，取显示值）
    QString m_rotateId;
    double m_rotateStartRotation = 0.0; // 旋转手势起点的显示角度（需求角度制）
    QRubberBand* m_rubberBand = nullptr;
};
