#pragma once

// 主场景：舞台背景（网格/舞台矩形/原点标记/台下准备区）+ 对象 Item。
// 舞台画在背景层，不可选中、不可交互；对象 Item 由 RuntimeData::fileData() 全量重建。
// 约定：1 场景单位 = 1cm，坐标 x 向右、y 向下，原点 = 舞台最里边中点。
// 阶段3：重建后按 SelectionModel 的 id 恢复选中高亮；向 StageView 提供命中查询。
// 阶段5：显示值与数据值分离——图元显示 TimelinePlayer 当前时刻插值结果
// （游标移动/播放 tick/fileDataChanged 后都刷新），而非静态 transform；
// 编辑命令被拒（打帧弹窗选"否"）时由 StageView 调 applyDisplayTransforms 回弹图元。
// 阶段10：台下准备区画在背景层（舞台色派生填充 + 描边 + 名称标签），主视口
// 只读展示；sceneRect/resetView 以 contentRect()（舞台 ∪ 准备区包围盒）为准。
// 阶段12：双帧选中态可视化——EditorCore::activeSegment 非空时绘制前帧幽灵
// （不透明）+ 后帧幽灵（0.4 透明）+ 两帧位置直线白线（恒宽 cosmetic）；
// 幽灵复用 ActorItem/ScreenItem 渲染（含朝向），打幽灵标记不参与选中/命中/
// 显示值驱动，位姿取关键帧数据值（与插值显示值分离）；态消失（nullopt）移除，
// 全量重建后按段引用重铺。

#include <QGraphicsScene>
#include <QRectF>

#include <optional>

#include "business/EditorCore.h" // SegmentRef

class RuntimeData;
class SelectionModel;
class StageObjectItem;
class TimelinePlayer;
class QGraphicsLineItem;

class StageScene : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit StageScene(RuntimeData* runtimeData, SelectionModel* selection,
                        TimelinePlayer* player, QObject* parent = nullptr);

    QRectF stageRect() const { return m_stageRect; }
    // 舞台 ∪ 台下准备区的包围盒（cm）：sceneRect 外扩与 resetView 视野中心以此为准
    QRectF contentRect() const;

    // 按 RuntimeData::fileData() 全量重建（fileDataChanged 触发），并恢复选中高亮
    void rebuild();

    // 按 TimelinePlayer 当前时刻值刷新全部图元的位置/朝向（不改数据）
    void applyDisplayTransforms();

    // 视口手势用的命中查询
    StageObjectItem* itemById(const QString& id) const;
    StageObjectItem* topStageItemAt(const QPointF& scenePos) const; // 最上层演员/屏风，无则空
    QList<StageObjectItem*> stageItems() const;

    // 双帧段选中态（阶段12）：设置/清除幽灵与白线覆盖物；nullopt = 态消失移除
    void setActiveSegment(const std::optional<SegmentRef>& segment);

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;

private:
    void applySelectionHighlight(); // 按 SelectionModel 的 id 刷新全部图元高亮
    void refreshSegmentOverlay();   // 按 m_segment 重建幽灵与白线（rebuild 后重铺）

    RuntimeData* m_runtimeData = nullptr; // 不拥有
    SelectionModel* m_selection = nullptr; // 不拥有
    TimelinePlayer* m_player = nullptr;    // 不拥有
    QRectF m_stageRect;                   // x ∈ [centerOffset−width/2, centerOffset+width/2]，y ∈ [0, depth]

    std::optional<SegmentRef> m_segment;       // 双帧段引用（运行时状态）
    QList<StageObjectItem*> m_segmentGhosts;   // 前帧（不透明）+ 后帧（0.4）幽灵
    QGraphicsLineItem* m_segmentLine = nullptr; // 两帧位置直线白线
};
