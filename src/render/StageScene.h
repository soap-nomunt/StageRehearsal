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

#include <QGraphicsScene>
#include <QRectF>

class RuntimeData;
class SelectionModel;
class StageObjectItem;
class TimelinePlayer;

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

protected:
    void drawBackground(QPainter* painter, const QRectF& rect) override;

private:
    void applySelectionHighlight(); // 按 SelectionModel 的 id 刷新全部图元高亮

    RuntimeData* m_runtimeData = nullptr; // 不拥有
    SelectionModel* m_selection = nullptr; // 不拥有
    TimelinePlayer* m_player = nullptr;    // 不拥有
    QRectF m_stageRect;                   // x ∈ [centerOffset−width/2, centerOffset+width/2]，y ∈ [0, depth]
};
