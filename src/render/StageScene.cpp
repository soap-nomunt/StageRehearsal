#include "StageScene.h"

#include "ActorItem.h"
#include "RenderUtil.h"
#include "ScreenItem.h"
#include "StageObjectItem.h"
#include "business/SelectionModel.h"
#include "business/TimelinePlayer.h"
#include "data/AppSettings.h"
#include "data/RuntimeData.h"

#include <QPainter>

#include <cmath>

StageScene::StageScene(RuntimeData* runtimeData, SelectionModel* selection,
                       TimelinePlayer* player, QObject* parent)
    : QGraphicsScene(parent)
    , m_runtimeData(runtimeData)
    , m_selection(selection)
    , m_player(player)
{
    connect(m_runtimeData, &RuntimeData::fileDataChanged, this, &StageScene::rebuild);
    connect(m_selection, &SelectionModel::selectionChanged,
            this, &StageScene::applySelectionHighlight);
    // 游标移动/播放 tick/数据重算后，图元跟随当前时刻插值结果
    connect(m_player, &TimelinePlayer::currentTimeChanged,
            this, [this](double) { applyDisplayTransforms(); });
    rebuild();
}

void StageScene::rebuild()
{
    clear();

    const FileData& file = m_runtimeData->fileData();
    for (const ObjectData& object : file.objects) {
        switch (object.type) {
        case ObjectType::Actor:
            addItem(new ActorItem(object));
            break;
        case ObjectType::Screen:
            addItem(new ScreenItem(object));
            break;
        case ObjectType::Prop:
            break; // 道具仅预留占位，本阶段不渲染
        case ObjectType::Group:
            break; // 空对象：纯组织节点，不进视口
        }
    }

    const StageData& stage = file.stage;
    m_stageRect = QRectF(stage.centerOffset - stage.width / 2.0, 0.0, stage.width, stage.depth);

    // 四周留出平移漫游余量，滚动条范围稳定
    const double margin = qMax(stage.width, stage.depth);
    setSceneRect(m_stageRect.adjusted(-margin, -margin, margin, margin));

    applySelectionHighlight(); // 全量重建后按 id 恢复选中高亮
    applyDisplayTransforms();  // 图元显示当前时刻插值结果（而非静态 transform）

    update(); // 背景依赖 StageData，触发重绘
}

void StageScene::applyDisplayTransforms()
{
    const FileData& file = m_runtimeData->fileData();
    for (const ObjectData& object : file.objects) {
        if (object.type != ObjectType::Actor && object.type != ObjectType::Screen)
            continue; // 空对象/道具不进视口
        StageObjectItem* item = itemById(object.id);
        if (!item)
            continue;
        const Transform value = TimelinePlayer::interpolate(object, m_player->currentTime());
        item->setPos(value.x, value.y);
        item->setRotation(RenderUtil::toQtRotation(value.rotation));
    }
}

StageObjectItem* StageScene::itemById(const QString& id) const
{
    for (QGraphicsItem* item : items()) {
        if (auto* stageItem = dynamic_cast<StageObjectItem*>(item)) {
            if (stageItem->objectId() == id)
                return stageItem;
        }
    }
    return nullptr;
}

StageObjectItem* StageScene::topStageItemAt(const QPointF& scenePos) const
{
    // items() 返回按叠放次序自上而下排列
    for (QGraphicsItem* item : items(scenePos)) {
        if (auto* stageItem = dynamic_cast<StageObjectItem*>(item))
            return stageItem;
    }
    return nullptr;
}

QList<StageObjectItem*> StageScene::stageItems() const
{
    QList<StageObjectItem*> result;
    for (QGraphicsItem* item : items()) {
        if (auto* stageItem = dynamic_cast<StageObjectItem*>(item))
            result.append(stageItem);
    }
    return result;
}

void StageScene::applySelectionHighlight()
{
    for (StageObjectItem* item : stageItems())
        item->setHighlighted(m_selection->isSelected(item->objectId()));
}

void StageScene::drawBackground(QPainter* painter, const QRectF& rect)
{
    // 画布底色（深灰）+ 舞台矩形（颜色取 AppSettings，可在"舞台"菜单调整）
    painter->fillRect(rect, QColor(QStringLiteral("#242424")));
    painter->fillRect(m_stageRect, m_runtimeData->settings().stageColor());

    // 网格：间距读用户设置（设置窗口可调，默认 100cm）；浅灰 cosmetic 笔宽恒为 1px，
    // 不随缩放变化；可见性由设置开关控制（仅影响显示，不影响吸附）
    if (m_runtimeData->settings().gridVisible()) {
        const double grid = m_runtimeData->settings().gridSizeCm();
        QPen gridPen(QColor(QStringLiteral("#d3d3d3")));
        gridPen.setCosmetic(true);
        painter->setPen(gridPen);
        for (double x = std::floor(rect.left() / grid) * grid; x <= rect.right(); x += grid)
            painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
        for (double y = std::floor(rect.top() / grid) * grid; y <= rect.bottom(); y += grid)
            painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
    }

    // 舞台描边压在网格之上
    QPen stagePen(QColor(QStringLiteral("#333333")));
    stagePen.setCosmetic(true);
    painter->setPen(stagePen);
    painter->setBrush(Qt::NoBrush);
    painter->drawRect(m_stageRect);

    // 原点标记：红十字 + 小圆（原点 = 舞台最里边中点 (0,0)）
    QPen originPen(QColor(QStringLiteral("#c0392b")));
    originPen.setCosmetic(true);
    painter->setPen(originPen);
    constexpr double armCm = 25.0;
    painter->drawLine(QPointF(-armCm, 0.0), QPointF(armCm, 0.0));
    painter->drawLine(QPointF(0.0, -armCm), QPointF(0.0, armCm));
    painter->drawEllipse(QPointF(0.0, 0.0), 5.0, 5.0);
}
