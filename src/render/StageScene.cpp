#include "StageScene.h"

#include "ActorItem.h"
#include "RenderUtil.h"
#include "ScreenItem.h"
#include "StageObjectItem.h"
#include "business/SelectionModel.h"
#include "business/TimelinePlayer.h"
#include "data/AppSettings.h"
#include "data/RuntimeData.h"

#include <QGraphicsLineItem>
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
    // clear() 连幽灵/白线一起删了：重置覆盖物句柄，末尾按段引用重铺
    m_segmentGhosts.clear();
    m_segmentLine = nullptr;

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

    // 四周留出平移漫游余量，滚动条范围稳定；包围盒含准备区（阶段10）
    const QRectF content = contentRect();
    const double margin = qMax(content.width(), content.height());
    setSceneRect(content.adjusted(-margin, -margin, margin, margin));

    applySelectionHighlight(); // 全量重建后按 id 恢复选中高亮
    applyDisplayTransforms();  // 图元显示当前时刻插值结果（而非静态 transform）
    refreshSegmentOverlay();   // 双帧态覆盖物随重建重铺（段引用悬空则由画布侧清除）

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

QRectF StageScene::contentRect() const
{
    QRectF content = m_stageRect;
    for (const PrepAreaData& area : m_runtimeData->fileData().prepAreas) {
        content = content.united(QRectF(area.x - area.width / 2.0, area.y - area.height / 2.0,
                                        area.width, area.height));
    }
    return content;
}

StageObjectItem* StageScene::itemById(const QString& id) const
{
    for (QGraphicsItem* item : items()) {
        if (auto* stageItem = dynamic_cast<StageObjectItem*>(item)) {
            if (!stageItem->isGhost() && stageItem->objectId() == id)
                return stageItem;
        }
    }
    return nullptr;
}

StageObjectItem* StageScene::topStageItemAt(const QPointF& scenePos) const
{
    // items() 返回按叠放次序自上而下排列；幽灵不参与命中
    for (QGraphicsItem* item : items(scenePos)) {
        if (auto* stageItem = dynamic_cast<StageObjectItem*>(item)) {
            if (!stageItem->isGhost())
                return stageItem;
        }
    }
    return nullptr;
}

QList<StageObjectItem*> StageScene::stageItems() const
{
    QList<StageObjectItem*> result;
    for (QGraphicsItem* item : items()) {
        if (auto* stageItem = dynamic_cast<StageObjectItem*>(item)) {
            if (!stageItem->isGhost()) // 幽灵不进选中/高亮/框选等对象遍历
                result.append(stageItem);
        }
    }
    return result;
}

void StageScene::applySelectionHighlight()
{
    for (StageObjectItem* item : stageItems())
        item->setHighlighted(m_selection->isSelected(item->objectId()));
}

// ---- 双帧选中态可视化（阶段12）----

void StageScene::setActiveSegment(const std::optional<SegmentRef>& segment)
{
    m_segment = segment;
    refreshSegmentOverlay();
}

void StageScene::refreshSegmentOverlay()
{
    // 先清旧覆盖物（QGraphicsItem 析构自动离场景）
    for (StageObjectItem* ghost : m_segmentGhosts)
        delete ghost;
    m_segmentGhosts.clear();
    delete m_segmentLine;
    m_segmentLine = nullptr;

    if (!m_segment)
        return;

    // 解析对象与两帧（防御：对象/帧缺失则整体不画；态清除由 TimelineCanvas 侧保证）
    const ObjectData* target = nullptr;
    for (const ObjectData& object : m_runtimeData->fileData().objects) {
        if (object.id == m_segment->objectId) {
            target = &object;
            break;
        }
    }
    if (!target)
        return;
    const Keyframe* fromKey = nullptr;
    const Keyframe* toKey = nullptr;
    for (const Keyframe& key : target->keyframes) {
        if (qAbs(key.t - m_segment->from) <= TimelinePlayer::kTimeEpsilon)
            fromKey = &key;
        if (qAbs(key.t - m_segment->to) <= TimelinePlayer::kTimeEpsilon)
            toKey = &key;
    }
    if (!fromKey || !toKey)
        return;

    // 直线白线：两端点 = 两帧位置，cosmetic 恒宽不随缩放变化；压在对象与幽灵之下
    QPen linePen(Qt::white);
    linePen.setCosmetic(true);
    linePen.setWidth(2);
    m_segmentLine = addLine(QLineF(QPointF(fromKey->x, fromKey->y),
                                   QPointF(toKey->x, toKey->y)),
                            linePen);
    m_segmentLine->setZValue(-2.0);

    // 幽灵：复用 ActorItem/ScreenItem 渲染（含朝向三角/凸点），位姿取关键帧数据值
    // （与插值显示值分离）；前帧不透明、后帧 0.4 透明；幽灵标记 → 选中/命中/显示值
    // 驱动全部跳过（itemById/topStageItemAt/stageItems 过滤）
    const auto makeGhost = [this, target](const Keyframe& key, qreal opacity) {
        StageObjectItem* ghost = nullptr;
        switch (target->type) {
        case ObjectType::Actor:  ghost = new ActorItem(*target);  break;
        case ObjectType::Screen: ghost = new ScreenItem(*target); break;
        default: return static_cast<StageObjectItem*>(nullptr); // 空对象/道具不渲染
        }
        ghost->setGhost(true);
        ghost->setPos(key.x, key.y);
        ghost->setRotation(RenderUtil::toQtRotation(key.rotation));
        ghost->setOpacity(opacity);
        ghost->setZValue(-1.0); // 白线之上、正常对象之下
        addItem(ghost);
        return ghost;
    };
    if (StageObjectItem* ghost = makeGhost(*fromKey, 1.0))
        m_segmentGhosts.append(ghost);
    if (StageObjectItem* ghost = makeGhost(*toKey, 0.4))
        m_segmentGhosts.append(ghost);
}

void StageScene::drawBackground(QPainter* painter, const QRectF& rect)
{
    // 画布底色（深灰）+ 舞台矩形（颜色取 AppSettings，可在"舞台"菜单调整）
    painter->fillRect(rect, QColor(QStringLiteral("#242424")));
    painter->fillRect(m_stageRect, m_runtimeData->settings().stageColor());

    // 台下准备区（阶段10）：主视口只读展示（背景层绘制，不可选中/拖动，
    // 只能经设置页修改）；填充逐区取色（阶段11）：独立色优先，invalid 派生态
    // 回退舞台色深色变体（与 PrepAreaCanvas 回退逻辑同式），压在网格之下
    for (const PrepAreaData& area : m_runtimeData->fileData().prepAreas) {
        const QColor fill = area.color.isValid() ? area.color
            : m_runtimeData->settings().stageColor().darker(160);
        painter->fillRect(QRectF(area.x - area.width / 2.0, area.y - area.height / 2.0,
                                 area.width, area.height),
                          fill);
    }

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

    // 准备区描边 + 名称标签（左上角）压在网格之上；标签字号恒定（抵消视图缩放）
    const double deviceScale = painter->worldTransform().m11(); // 场景 → 设备缩放
    QPen prepPen(QColor(QStringLiteral("#4a4a4a")));
    prepPen.setCosmetic(true);
    for (const PrepAreaData& area : m_runtimeData->fileData().prepAreas) {
        const QRectF areaRect(area.x - area.width / 2.0, area.y - area.height / 2.0,
                              area.width, area.height);
        painter->setPen(prepPen);
        painter->drawRect(areaRect);
        painter->save();
        painter->translate(areaRect.topLeft());
        if (deviceScale > 1e-9)
            painter->scale(1.0 / deviceScale, 1.0 / deviceScale);
        painter->setPen(QColor(QStringLiteral("#e8e8e8")));
        painter->drawText(QPointF(6.0, 18.0), area.name);
        painter->restore();
    }

    // 原点标记：红十字 + 小圆（原点 = 舞台最里边中点 (0,0)）
    QPen originPen(QColor(QStringLiteral("#c0392b")));
    originPen.setCosmetic(true);
    painter->setPen(originPen);
    constexpr double armCm = 25.0;
    painter->drawLine(QPointF(-armCm, 0.0), QPointF(armCm, 0.0));
    painter->drawLine(QPointF(0.0, -armCm), QPointF(0.0, armCm));
    painter->drawEllipse(QPointF(0.0, 0.0), 5.0, 5.0);
}
