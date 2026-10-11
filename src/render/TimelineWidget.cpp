#include "TimelineWidget.h"

#include "business/EditorCore.h"
#include "business/SelectionModel.h"
#include "business/TimelinePlayer.h"
#include "data/RuntimeData.h"

#include <QApplication>
#include <QComboBox>
#include <QContextMenuEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <cmath>

namespace {

constexpr int kTrackRowH = 26;  // 对象轨道行高
constexpr int kGroupRowH = 20;  // 同类分组标题行高
constexpr int kRightPad = 40;   // 时长右侧留白（px）
constexpr double kMinPxPerSec = 2.0;
constexpr double kMaxPxPerSec = 200.0;
constexpr double kKeyframeHitRadius = 7.0; // 关键帧点命中半径（px）
constexpr double kCursorHitRadius = 5.0;   // 游标手柄/线命中半径（px）
constexpr double kDragSnapSec = 0.1;       // 拖动关键帧/场次标签的时刻吸附步长（秒）
constexpr double kScrubSnapSec = 0.5;      // 游标按下/拖拽的时刻吸附步长（秒）
constexpr double kArrowStepSec = 0.5;      // ←/→ 方向键步进步长（秒，与游标拖拽吸附一致）

QString groupTitle(ObjectType type, int count)
{
    return QStringLiteral("%1（%2）")
        .arg(type == ObjectType::Actor ? QStringLiteral("演员") : QStringLiteral("屏风"))
        .arg(count);
}

} // namespace

// 时间轴画布：标尺/场次标签行/轨道/关键帧点/游标的绘制与鼠标编辑。
// 横向滚动由外层 QScrollArea 提供；绘制时按滚动偏移把左侧名列钉住（冻结列）。
// 画布统一选择模型（局部状态）：游标/关键帧/场次标签/轨道四类互斥选中，
// 轨道选中与对象 SelectionModel 联动；命中优先级：关键帧点 > 场次标签 >
// 游标手柄/线 > 轨道行 > 空白。
class TimelineCanvas : public QWidget {
public:
    TimelineCanvas(EditorCore* core, QScrollArea* scrollOwner, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_core(core)
        , m_scrollOwner(scrollOwner)
    {
        setFocusPolicy(Qt::StrongFocus); // Delete 删除选中关键帧需要键盘焦点
        setMouseTracking(false);
        rebuildRows();
    }

    double pxPerSec() const { return m_pxPerSec; }

    double timeToX(double t) const { return TimelineWidget::kNameColW + t * m_pxPerSec; }
    double xToTime(double x) const { return (x - TimelineWidget::kNameColW) / m_pxPerSec; }

    // 供自测定位：对象轨道行中心 y（无轨道返回 -1）
    int trackRowCenterY(const QString& objectId) const
    {
        for (const Row& row : m_rows) {
            if (row.kind == Row::Kind::Track && row.objectId == objectId)
                return row.top + row.height / 2;
        }
        return -1;
    }

    void rebuildRows()
    {
        m_rows.clear();
        m_rows.append(Row { Row::Kind::Ruler, ObjectType::Actor, QString(), QString(), 0, TimelineWidget::kRulerH });
        m_rows.append(Row { Row::Kind::Labels, ObjectType::Actor, QString(), QString(), 0, TimelineWidget::kLabelRowH });

        const FileData& file = m_core->runtimeData()->fileData();
        for (ObjectType type : { ObjectType::Actor, ObjectType::Screen }) {
            int count = 0;
            for (const ObjectData& object : file.objects)
                count += object.type == type ? 1 : 0;
            Row header { Row::Kind::GroupHeader, type, QString(), groupTitle(type, count), 0, kGroupRowH };
            m_rows.append(header);
            if (m_collapsedGroups.contains(objectTypeToString(type)))
                continue; // 折叠：只留分组标题行
            for (const ObjectData& object : file.objects) {
                if (object.type == type)
                    m_rows.append(Row { Row::Kind::Track, type, object.id, object.name, 0, kTrackRowH });
            }
        }

        int y = 0;
        for (Row& row : m_rows) {
            row.top = y;
            y += row.height;
        }
        setMinimumSize(TimelineWidget::kNameColW + int(file.timeline.duration * m_pxPerSec) + kRightPad,
                       y);

        // 剔除已不存在的关键帧选中（撤销/删除后）
        QList<QPair<QString, double>> kept;
        for (const QPair<QString, double>& ref : m_selectedKeys) {
            const ObjectData* object = m_core->findObject(ref.first);
            if (!object)
                continue;
            bool exists = false;
            for (const Keyframe& key : object->keyframes)
                exists = exists || qAbs(key.t - ref.second) <= TimelinePlayer::kTimeEpsilon;
            if (exists)
                kept.append(ref);
        }
        m_selectedKeys = kept;
        if (m_sel == CanvasSel::Keys && m_selectedKeys.isEmpty())
            m_sel = CanvasSel::None;
        // 标签选中剔除（撤销/删除后下标越界）
        if (m_sel == CanvasSel::Label
            && (m_selectedLabel < 0
                || m_selectedLabel >= file.timeline.labels.size())) {
            m_selectedLabel = -1;
            m_sel = CanvasSel::None;
        }

        update();
    }

    // 对象 SelectionModel → 画布轨道选中联动：
    // 外部单选了有轨道的对象 → 画布同步选中该轨道（清其它画布选择）；
    // 多选/空选/无轨道对象 → 清除轨道选中。选择游标/关键帧/标签不清对象选中
    // （Inspector/视口联动保持），反向只在 m_sel==Track 时回收。
    void syncTrackSelection()
    {
        const QString id = m_core->selection()->singleSelectedId();
        const bool hasTrack = !id.isEmpty() && trackRowCenterY(id) >= 0;
        if (hasTrack) {
            m_sel = CanvasSel::Track;
            m_selectedTrackId = id;
            m_selectedKeys.clear();
            m_selectedLabel = -1;
        } else if (m_sel == CanvasSel::Track) {
            m_sel = CanvasSel::None;
            m_selectedTrackId.clear();
        }
        update();
    }

protected:
    void paintEvent(QPaintEvent* /*event*/) override
    {
        QPainter painter(this);
        const int w = width();
        const int h = height();
        painter.fillRect(rect(), QColor(QStringLiteral("#2b2b2b")));

        const FileData& file = m_core->runtimeData()->fileData();
        const double duration = file.timeline.duration;

        // ---- 行背景 ----
        int trackIndex = 0;
        for (const Row& row : m_rows) {
            const QRect rect(0, row.top, w, row.height);
            switch (row.kind) {
            case Row::Kind::Ruler:
                painter.fillRect(rect, QColor(QStringLiteral("#262626")));
                break;
            case Row::Kind::Labels:
                painter.fillRect(rect, QColor(QStringLiteral("#24292e")));
                break;
            case Row::Kind::GroupHeader:
                painter.fillRect(rect, QColor(QStringLiteral("#383c42")));
                break;
            case Row::Kind::Track:
                painter.fillRect(rect, (trackIndex++ % 2 == 0)
                                           ? QColor(QStringLiteral("#2f2f2f"))
                                           : QColor(QStringLiteral("#333333")));
                break;
            }
            painter.setPen(QColor(QStringLiteral("#1d1d1d")));
            painter.drawLine(0, row.top + row.height - 1, w, row.top + row.height - 1);
        }

        // ---- 标尺刻度：自适应步长（主刻度间距 ≥ 45px），含次刻度 ----
        {
            const Row& ruler = m_rows.at(0);
            double step = 60.0;
            for (double s : { 0.1, 0.2, 0.5, 1.0, 2.0, 5.0, 10.0, 15.0, 30.0, 60.0 }) {
                if (s * m_pxPerSec >= 45.0) {
                    step = s;
                    break;
                }
            }
            const double minor = step / 5.0;
            const bool drawMinor = minor * m_pxPerSec >= 6.0;
            QPen tickPen(QColor(QStringLiteral("#8a8a8a")));
            tickPen.setCosmetic(true);
            painter.setPen(tickPen);
            const int decimals = step < 1.0 ? 1 : 0;
            for (double t = 0.0; t <= duration + 1e-9; t += minor) {
                const int x = int(timeToX(t));
                const bool major = qAbs(t / step - std::round(t / step)) < 1e-6;
                if (!major && !drawMinor)
                    continue;
                painter.drawLine(x, ruler.top + ruler.height - (major ? 12 : 6),
                                 x, ruler.top + ruler.height);
                if (major)
                    painter.drawText(x + 3, ruler.top + 13,
                                     QString::number(t, 'f', decimals) + QStringLiteral("s"));
            }
            // 时长终点线
            painter.setPen(QColor(QStringLiteral("#5a5a5a")));
            const int endX = int(timeToX(duration));
            painter.drawLine(endX, 0, endX, h);
        }

        // ---- 场次标签行 ----
        {
            const Row& labels = m_rows.at(1);
            for (int i = 0; i < file.timeline.labels.size(); ++i) {
                const SceneLabel& label = file.timeline.labels.at(i);
                const bool dragged = m_labelDragging && i == m_labelDragIndex;
                const int x = int(timeToX(dragged ? m_labelGhostT : label.t));
                const bool selected = m_sel == CanvasSel::Label && i == m_selectedLabel;
                if (selected) {
                    // 选中高亮框（旗标 + 文本外框）
                    const double textW =
                        painter.fontMetrics().horizontalAdvance(label.name);
                    painter.setPen(QPen(QColor(QStringLiteral("#ffd54f")), 1.0));
                    painter.setBrush(Qt::NoBrush);
                    painter.drawRect(QRectF(x - 3.0, labels.top + 1.0, 16.0 + textW,
                                            labels.height - 2.0));
                }
                painter.setPen(Qt::NoPen);
                painter.setBrush(dragged ? QColor(QStringLiteral("#f39c12")) // 拖动中的幽灵标签
                                         : QColor(QStringLiteral("#4a90d9")));
                const QPolygonF flag { QPointF(x, labels.top + 3.0),
                                       QPointF(x + 10.0, labels.top + 8.0),
                                       QPointF(x, labels.top + 13.0) };
                painter.drawPolygon(flag);
                painter.setPen(QColor(QStringLiteral("#9ecbff")));
                painter.drawText(QRectF(x + 13.0, labels.top, 200.0, labels.height),
                                 Qt::AlignVCenter, label.name);
            }
        }

        // ---- 关键帧点 ----
        for (const Row& row : m_rows) {
            if (row.kind != Row::Kind::Track)
                continue;
            const ObjectData* object = m_core->findObject(row.objectId);
            if (!object)
                continue;
            const int cy = row.top + row.height / 2;
            for (const Keyframe& key : object->keyframes) {
                const bool dragged =
                    m_keyDragging && m_dragId == row.objectId
                    && qAbs(key.t - m_dragOldT) <= TimelinePlayer::kTimeEpsilon;
                const int x = int(timeToX(dragged ? m_dragGhostT : key.t));
                const bool selected = isKeySelected(row.objectId, key.t);
                const QPolygonF diamond { QPointF(x, cy - 6.0), QPointF(x + 6.0, cy),
                                          QPointF(x, cy + 6.0), QPointF(x - 6.0, cy) };
                painter.setPen(QPen(selected ? QColor(Qt::white)
                                             : QColor(QStringLiteral("#202020")),
                                    selected ? 2.0 : 1.0));
                QColor fill = object->color.isValid() ? object->color
                                                      : QColor(QStringLiteral("#f1c40f"));
                if (dragged)
                    fill = QColor(QStringLiteral("#f39c12")); // 拖动中的幽灵帧
                painter.setBrush(fill);
                painter.drawPolygon(diamond);
            }
        }

        // ---- 游标（选中态加粗/亮色）----
        {
            const bool cursorSelected = m_sel == CanvasSel::Cursor;
            const QColor color(cursorSelected ? QStringLiteral("#ff8a65")
                                              : QStringLiteral("#e74c3c"));
            const int x = int(timeToX(m_core->timelinePlayer()->currentTime()));
            QPen pen(color);
            pen.setWidth(cursorSelected ? 3 : 2);
            painter.setPen(pen);
            painter.drawLine(x, 0, x, h);
            painter.setPen(Qt::NoPen);
            painter.setBrush(color);
            const double hw = cursorSelected ? 8.0 : 6.0; // 手柄半宽
            painter.drawPolygon(QPolygonF { QPointF(x - hw, 0.0), QPointF(x + hw, 0.0),
                                            QPointF(x, cursorSelected ? 12.0 : 9.0) });
        }

        // ---- 冻结名列（随横向滚动钉住）----
        const int leftEdge =
            m_scrollOwner ? m_scrollOwner->horizontalScrollBar()->value() : 0;
        painter.save();
        painter.translate(leftEdge, 0);
        painter.fillRect(QRect(0, 0, TimelineWidget::kNameColW, h),
                         QColor(QStringLiteral("#222629")));
        QFont bold = painter.font();
        bold.setBold(true);
        for (const Row& row : m_rows) {
            const QRect cell(0, row.top, TimelineWidget::kNameColW - 4, row.height);
            switch (row.kind) {
            case Row::Kind::Ruler:
                painter.setPen(QColor(QStringLiteral("#777777")));
                painter.drawText(cell, Qt::AlignVCenter | Qt::AlignRight,
                                 QStringLiteral("时间 "));
                break;
            case Row::Kind::Labels:
                painter.setPen(QColor(QStringLiteral("#777777")));
                painter.drawText(cell, Qt::AlignVCenter | Qt::AlignRight,
                                 QStringLiteral("场次标签 "));
                break;
            case Row::Kind::GroupHeader: {
                painter.setFont(bold);
                painter.setPen(QColor(QStringLiteral("#dddddd")));
                const bool collapsed =
                    m_collapsedGroups.contains(objectTypeToString(row.group));
                painter.drawText(cell.adjusted(6, 0, 0, 0), Qt::AlignVCenter,
                                 (collapsed ? QStringLiteral("▸ ")
                                            : QStringLiteral("▾ ")) + row.title);
                painter.setFont(QFont());
                break;
            }
            case Row::Kind::Track: {
                const ObjectData* object = m_core->findObject(row.objectId);
                const bool selected = m_core->selection()->isSelected(row.objectId);
                if (selected)
                    painter.fillRect(cell, QColor(QStringLiteral("#2d3a4a")));
                if (object && object->color.isValid()) {
                    painter.setPen(Qt::NoPen);
                    painter.setBrush(object->color);
                    painter.drawEllipse(QPointF(12.0, row.top + row.height / 2.0), 5.0, 5.0);
                }
                painter.setPen(QColor(QStringLiteral("#cccccc")));
                painter.drawText(cell.adjusted(22, 0, 0, 0), Qt::AlignVCenter,
                                 painter.fontMetrics().elidedText(row.title, Qt::ElideRight,
                                                                  cell.width() - 24));
                break;
            }
            }
        }
        painter.setPen(QColor(QStringLiteral("#111111")));
        painter.drawLine(TimelineWidget::kNameColW - 1, 0, TimelineWidget::kNameColW - 1, h);
        painter.restore();
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() != Qt::LeftButton) {
            QWidget::mousePressEvent(event);
            return;
        }
        setFocus();
        m_pressPos = event->position().toPoint();
        const Row* row = rowAtY(m_pressPos.y());
        const bool ctrl = event->modifiers().testFlag(Qt::ControlModifier);

        if (inNameCol(m_pressPos.x())) {
            // 冻结名列：分组标题切换折叠；轨道名点击选中轨道（联动对象选中）
            if (row && row->kind == Row::Kind::GroupHeader) {
                const QString key = objectTypeToString(row->group);
                if (m_collapsedGroups.contains(key))
                    m_collapsedGroups.remove(key);
                else
                    m_collapsedGroups.insert(key);
                rebuildRows();
            } else if (row && row->kind == Row::Kind::Track) {
                selectTrack(row->objectId);
            }
            return;
        }

        if (!row)
            return;

        // 命中优先级（自上而下）：关键帧点 > 场次标签 > 游标手柄/线 > 轨道行 > 空白
        if (row->kind == Row::Kind::Track) {
            QString id;
            double keyT = 0.0;
            if (keyframeAt(m_pressPos, id, keyT)) {
                if (ctrl) {
                    toggleKeySelection(id, keyT); // Ctrl 追加/反选多选，不进入拖动
                } else {
                    if (!isKeySelected(id, keyT))
                        selectKeys({ qMakePair(id, keyT) });
                    // 长按拖动待命（拖动只作用于按下的这一帧）
                    m_keyDragArmed = true;
                    m_dragId = id;
                    m_dragOldT = keyT;
                    m_dragGhostT = keyT;
                }
            } else if (cursorHit(m_pressPos)) {
                selectCursor();
                m_scrubbing = true; // 游标线命中：按下即定位（0.5s 吸附）
                scrubToX(m_pressPos.x());
            } else if (!ctrl) {
                selectTrack(row->objectId); // 轨道行空白 = 选中轨道（不再清除一切）
            }
            return;
        }

        if (row->kind == Row::Kind::Labels) {
            const int index = labelIndexAt(m_pressPos);
            if (index >= 0) {
                selectLabel(index);
                // 长按拖动待命（吸附 0.1s，与关键帧一致）
                m_labelDragArmed = true;
                m_labelDragIndex = index;
                m_labelGhostT =
                    m_core->runtimeData()->fileData().timeline.labels.at(index).t;
                return;
            }
            // 标签行空白：照常 seek + 选中游标
            selectCursor();
            m_scrubbing = true;
            scrubToX(m_pressPos.x());
            return;
        }

        if (row->kind == Row::Kind::Ruler) {
            // 标尺按下即定位并选中游标，拖动实时 seekTo（全程 0.5s 吸附）
            selectCursor();
            m_scrubbing = true;
            scrubToX(m_pressPos.x());
        }
        // GroupHeader 行时间区：无操作
    }

    void mouseMoveEvent(QMouseEvent* event) override
    {
        const QPoint pos = event->position().toPoint();
        if (m_scrubbing) {
            scrubToX(pos.x());
            return;
        }
        if (m_keyDragArmed
            && (pos - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
            m_keyDragArmed = false;
            m_keyDragging = true;
        }
        if (m_keyDragging) {
            m_dragGhostT = snapTime(qMax(0.0, xToTime(pos.x())));
            update();
            return;
        }
        if (m_labelDragArmed
            && (pos - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
            m_labelDragArmed = false;
            m_labelDragging = true;
        }
        if (m_labelDragging) {
            m_labelGhostT = snapTime(qMax(0.0, xToTime(pos.x())));
            update();
        }
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() != Qt::LeftButton) {
            QWidget::mouseReleaseEvent(event);
            return;
        }
        m_scrubbing = false;
        if (m_keyDragging) {
            // 松开一次性提交改时刻命令（吸附 0.1s；无变更由入口丢弃），选中跟随新时刻
            if (m_core->moveKeyframe(m_dragId, m_dragOldT, m_dragGhostT)) {
                m_selectedKeys.append(qMakePair(m_dragId, m_dragGhostT)); // 旧引用已由 rebuildRows 剔除
                m_sel = CanvasSel::Keys;
            }
            m_keyDragging = false;
        }
        m_keyDragArmed = false;
        if (m_labelDragging) {
            moveLabelAt(m_labelDragIndex, m_labelGhostT);
            m_labelDragging = false;
        }
        m_labelDragArmed = false;
        update();
    }

    void mouseDoubleClickEvent(QMouseEvent* event) override
    {
        // 双击场次标签 → 改名（命令入撤销栈）
        const int index = labelIndexAt(event->position().toPoint());
        if (index >= 0) {
            const SceneLabel& label =
                m_core->runtimeData()->fileData().timeline.labels.at(index);
            bool ok = false;
            const QString name = QInputDialog::getText(
                this, QStringLiteral("重命名场次标签"), QStringLiteral("标签名称"),
                QLineEdit::Normal, label.name, &ok);
            if (ok)
                m_core->renameSceneLabel(index, name);
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }

    void contextMenuEvent(QContextMenuEvent* event) override
    {
        const QPoint pos = event->pos();

        // 关键帧右键：复制 / 粘贴（游标处，剪贴板空则禁用）/ 删除（作用于当前选中集）
        QString keyId;
        double keyT = 0.0;
        if (keyframeAt(pos, keyId, keyT)) {
            if (!isKeySelected(keyId, keyT))
                selectKeys({ qMakePair(keyId, keyT) }); // 右键未选中帧 → 先单选它
            QMenu menu(this);
            QAction* copyAction = menu.addAction(QStringLiteral("复制(&C)"));
            QAction* pasteAction = menu.addAction(QStringLiteral("粘贴(&P)"));
            pasteAction->setEnabled(m_core->keyframeClipboardHasContent());
            QAction* removeAction = menu.addAction(QStringLiteral("删除(&D)"));
            QAction* chosen = menu.exec(event->globalPos());
            if (chosen == copyAction)
                m_core->copySelectedKeyframes(m_selectedKeys);
            else if (chosen == pasteAction)
                m_core->pasteKeyframesAtCursor();
            else if (chosen == removeAction)
                m_core->removeKeyframes(m_selectedKeys);
            return;
        }

        // 场次标签右键：重命名 / 删除（沿用）
        const int index = labelIndexAt(pos);
        if (index >= 0) {
            QMenu menu(this);
            QAction* renameAction = menu.addAction(QStringLiteral("重命名(&R)..."));
            QAction* removeAction = menu.addAction(QStringLiteral("删除(&D)"));
            QAction* chosen = menu.exec(event->globalPos());
            if (chosen == renameAction) {
                const SceneLabel& label =
                    m_core->runtimeData()->fileData().timeline.labels.at(index);
                bool ok = false;
                const QString name = QInputDialog::getText(
                    this, QStringLiteral("重命名场次标签"), QStringLiteral("标签名称"),
                    QLineEdit::Normal, label.name, &ok);
                if (ok)
                    m_core->renameSceneLabel(index, name);
            } else if (chosen == removeAction) {
                m_core->removeSceneLabel(index);
            }
            return;
        }

        // 轨道右键（名列或轨道行时间区）：全部转发既有 EditorCore 入口
        const Row* row = rowAtY(pos.y());
        if (row && row->kind == Row::Kind::Track) {
            const QString objectId = row->objectId;
            QMenu menu(this);
            QAction* selectAction = menu.addAction(QStringLiteral("选中该对象(&S)"));
            QAction* keyframeAction = menu.addAction(QStringLiteral("在游标处打帧(&K)"));
            QAction* copyAction = menu.addAction(QStringLiteral("复制对象(&C)"));
            QAction* removeAction = menu.addAction(QStringLiteral("删除对象(&D)"));
            QAction* chosen = menu.exec(event->globalPos());
            if (chosen == selectAction) {
                selectTrack(objectId);
            } else if (chosen == keyframeAction) {
                // 打帧走 keyframeSelectionAtCursor：先把该对象设为单选再调
                m_core->selection()->select(objectId);
                m_core->keyframeSelectionAtCursor();
            } else if (chosen == copyAction) {
                m_core->selection()->select(objectId);
                m_core->copySelection();
            } else if (chosen == removeAction) {
                m_core->selection()->select(objectId);
                m_core->deleteSelection();
            }
            return;
        }

        QWidget::contextMenuEvent(event);
    }

    void keyPressEvent(QKeyEvent* event) override
    {
        const bool ctrl = event->modifiers().testFlag(Qt::ControlModifier);
        switch (event->key()) {
        case Qt::Key_Delete:
            if (m_sel == CanvasSel::Keys && !m_selectedKeys.isEmpty()) {
                m_core->removeKeyframes(m_selectedKeys); // 批量一条命令，一步撤销
                return;
            }
            if (m_sel == CanvasSel::Label) {
                m_core->removeSceneLabel(m_selectedLabel); // 选中态由 rebuildRows 剔除
                return;
            }
            break;
        case Qt::Key_Left:
        case Qt::Key_Right: {
            // 按画布选择类型分发 ±0.5s 步进（与游标拖拽吸附一致）；轨道/无选择不处理，交给上层
            const double delta =
                (event->key() == Qt::Key_Left ? -1.0 : 1.0) * kArrowStepSec;
            if (m_sel == CanvasSel::Cursor) {
                TimelinePlayer* player = m_core->timelinePlayer();
                player->seekTo(player->currentTime() + delta); // clamp [0,duration]，播放中也可用
                return;
            }
            if (m_sel == CanvasSel::Keys) {
                stepSelectedKeys(delta);
                return;
            }
            if (m_sel == CanvasSel::Label) {
                const QList<SceneLabel>& labels =
                    m_core->runtimeData()->fileData().timeline.labels;
                if (m_selectedLabel >= 0 && m_selectedLabel < labels.size())
                    moveLabelAt(m_selectedLabel, labels.at(m_selectedLabel).t + delta);
                return;
            }
            break;
        }
        case Qt::Key_C:
            if (ctrl && m_sel == CanvasSel::Keys && !m_selectedKeys.isEmpty()) {
                m_core->copySelectedKeyframes(m_selectedKeys); // 关键帧级剪贴板
                return;
            }
            break;
        case Qt::Key_V:
            if (ctrl && m_core->keyframeClipboardHasContent()) {
                m_core->pasteKeyframesAtCursor(); // 最小 t 对齐游标，保持相对间隔
                return;
            }
            break;
        case Qt::Key_Escape:
            clearCanvasSelection();
            return;
        default:
            break;
        }
        QWidget::keyPressEvent(event);
    }

    void wheelEvent(QWheelEvent* event) override
    {
        if (event->modifiers().testFlag(Qt::ControlModifier)) {
            // Ctrl+滚轮缩放时间轴（保持鼠标下时刻不动）
            const double anchorT = xToTime(event->position().x());
            const double factor =
                std::pow(1.25, event->angleDelta().y() / 120.0);
            m_pxPerSec = qBound(kMinPxPerSec, m_pxPerSec * factor, kMaxPxPerSec);
            rebuildRows();
            if (m_scrollOwner) {
                const int target =
                    int(timeToX(anchorT) - event->position().x());
                m_scrollOwner->horizontalScrollBar()->setValue(target);
            }
            event->accept();
            return;
        }
        QWidget::wheelEvent(event); // 交给滚动区（纵向滚动）
    }

    // 画布快捷键仲裁（只拦"画布选择非空"的情形，否则落回编辑菜单对象语义）：
    // Delete：关键帧/场次标签选中时归画布；Ctrl+C：关键帧选中时归画布；
    // Ctrl+V：关键帧剪贴板非空时归画布。不接受 ShortcutOverride 则不拦截，
    // 主窗口编辑菜单（对象级复制/粘贴/删除）与视口方向键行为不变
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::ShortcutOverride) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            const bool ctrl = keyEvent->modifiers().testFlag(Qt::ControlModifier);
            const bool own =
                (keyEvent->key() == Qt::Key_Delete
                 && (m_sel == CanvasSel::Keys || m_sel == CanvasSel::Label))
                || (keyEvent->key() == Qt::Key_C && ctrl && m_sel == CanvasSel::Keys)
                || (keyEvent->key() == Qt::Key_V && ctrl
                    && m_core->keyframeClipboardHasContent());
            if (own) {
                event->accept();
                return true;
            }
        }
        return QWidget::event(event);
    }

private:
    struct Row {
        enum class Kind { Ruler, Labels, GroupHeader, Track };
        Kind kind;
        ObjectType group;   // GroupHeader 有效
        QString objectId;   // Track 有效
        QString title;
        int top;
        int height;
    };

    // 画布选择类别：同一时刻只有一类非空（关键帧多选内部共存）
    enum class CanvasSel { None, Cursor, Keys, Label, Track };

    static double snapTime(double t) // 拖动吸附（关键帧/标签，0.1s）
    {
        return std::round(t / kDragSnapSec) * kDragSnapSec;
    }

    // 游标按下/拖拽：0.5s 吸附后 seek（seekTo 内部 clamp [0, duration]）
    void scrubToX(int x)
    {
        const double t = qMax(0.0, xToTime(x));
        m_core->timelinePlayer()->seekTo(std::round(t / kScrubSnapSec) * kScrubSnapSec);
    }

    // 游标手柄/线命中（全高度，容差 5px）
    bool cursorHit(const QPoint& pos) const
    {
        return qAbs(pos.x() - timeToX(m_core->timelinePlayer()->currentTime()))
            <= kCursorHitRadius;
    }

    // 冻结名列命中按其实际绘制位置判定（随横向滚动钉住）
    bool inNameCol(int x) const
    {
        const int leftEdge =
            m_scrollOwner ? m_scrollOwner->horizontalScrollBar()->value() : 0;
        return x >= leftEdge && x < leftEdge + TimelineWidget::kNameColW;
    }

    // ---- 画布选择切换（四类互斥：选中一类清空其它类）----
    void selectCursor()
    {
        m_sel = CanvasSel::Cursor;
        m_selectedKeys.clear();
        m_selectedLabel = -1;
        m_selectedTrackId.clear();
        update();
    }

    void selectKeys(const QList<QPair<QString, double>>& refs)
    {
        m_sel = refs.isEmpty() ? CanvasSel::None : CanvasSel::Keys;
        m_selectedKeys = refs;
        m_selectedLabel = -1;
        m_selectedTrackId.clear();
        update();
    }

    void selectLabel(int index)
    {
        m_sel = CanvasSel::Label;
        m_selectedLabel = index;
        m_selectedKeys.clear();
        m_selectedTrackId.clear();
        update();
    }

    void selectTrack(const QString& objectId)
    {
        m_sel = CanvasSel::Track;
        m_selectedTrackId = objectId;
        m_selectedKeys.clear();
        m_selectedLabel = -1;
        m_core->selection()->select(objectId); // 联动对象选中（Hierarchy/Inspector/视口）
        update();
    }

    // Esc 清空画布选择；轨道选中即对象选中，一并联动清空
    void clearCanvasSelection()
    {
        const bool hadTrack = m_sel == CanvasSel::Track;
        m_sel = CanvasSel::None;
        m_selectedKeys.clear();
        m_selectedLabel = -1;
        m_selectedTrackId.clear();
        if (hadTrack)
            m_core->selection()->clear();
        update();
    }

    // ←/→ 批量步进：一条批量命令一步撤销；选中跟随新时刻（被覆盖消失的帧剔除）
    void stepSelectedKeys(double deltaT)
    {
        if (m_selectedKeys.isEmpty())
            return;
        QList<QPair<QString, double>> movedRefs;
        for (const QPair<QString, double>& ref : m_selectedKeys)
            movedRefs.append(qMakePair(ref.first, qMax(0.0, ref.second + deltaT)));
        if (!m_core->moveKeyframes(m_selectedKeys, deltaT))
            return;
        // 提交时 rebuildRows 已剔除旧引用；以新时刻重建选中
        QList<QPair<QString, double>> kept;
        for (const QPair<QString, double>& ref : movedRefs) {
            const ObjectData* object = m_core->findObject(ref.first);
            if (!object)
                continue;
            bool exists = false;
            for (const Keyframe& key : object->keyframes)
                exists = exists || qAbs(key.t - ref.second) <= TimelinePlayer::kTimeEpsilon;
            if (exists && !kept.contains(ref))
                kept.append(ref);
        }
        m_selectedKeys = kept;
        m_sel = kept.isEmpty() ? CanvasSel::None : CanvasSel::Keys;
        update();
    }

    // 移动标签到 newT（拖动松开/方向键步进共用）；命令内部按 t 重排，
    // 按 (新时刻, 原名) 找回下标使选中跟随
    void moveLabelAt(int index, double newT)
    {
        const QList<SceneLabel>& labels =
            m_core->runtimeData()->fileData().timeline.labels;
        if (index < 0 || index >= labels.size())
            return;
        const QString name = labels.at(index).name;
        if (!m_core->moveSceneLabel(index, newT))
            return;
        const QList<SceneLabel>& after =
            m_core->runtimeData()->fileData().timeline.labels;
        const double clamped = qMax(0.0, newT);
        for (int i = 0; i < after.size(); ++i) {
            if (after.at(i).name == name
                && qAbs(after.at(i).t - clamped) <= TimelinePlayer::kTimeEpsilon) {
                m_selectedLabel = i;
                break;
            }
        }
        m_sel = CanvasSel::Label;
        update();
    }

    const Row* rowAtY(int y) const
    {
        for (const Row& row : m_rows) {
            if (y >= row.top && y < row.top + row.height)
                return &row;
        }
        return nullptr;
    }

    bool isKeySelected(const QString& id, double t) const
    {
        for (const QPair<QString, double>& ref : m_selectedKeys) {
            if (ref.first == id && qAbs(ref.second - t) <= TimelinePlayer::kTimeEpsilon)
                return true;
        }
        return false;
    }

    void toggleKeySelection(const QString& id, double t)
    {
        QList<QPair<QString, double>> refs =
            m_sel == CanvasSel::Keys ? m_selectedKeys : QList<QPair<QString, double>>();
        for (int i = 0; i < refs.size(); ++i) {
            if (refs.at(i).first == id
                && qAbs(refs.at(i).second - t) <= TimelinePlayer::kTimeEpsilon) {
                refs.removeAt(i);
                selectKeys(refs);
                return;
            }
        }
        refs.append(qMakePair(id, t));
        selectKeys(refs);
    }

    // 命中轨道上的关键帧点（容差 7px）
    bool keyframeAt(const QPoint& pos, QString& idOut, double& tOut) const
    {
        const Row* row = rowAtY(pos.y());
        if (!row || row->kind != Row::Kind::Track)
            return false;
        const ObjectData* object = m_core->findObject(row->objectId);
        if (!object)
            return false;
        for (const Keyframe& key : object->keyframes) {
            if (qAbs(timeToX(key.t) - pos.x()) <= kKeyframeHitRadius) {
                idOut = row->objectId;
                tOut = key.t;
                return true;
            }
        }
        return false;
    }

    // 命中场次标签（旗标 + 文本框），返回标签下标，未命中 -1
    int labelIndexAt(const QPoint& pos) const
    {
        if (m_rows.size() < 2)
            return -1;
        const Row& labels = m_rows.at(1);
        if (pos.y() < labels.top || pos.y() >= labels.top + labels.height)
            return -1;
        const QList<SceneLabel>& all =
            m_core->runtimeData()->fileData().timeline.labels;
        const QFontMetrics metrics(const_cast<TimelineCanvas*>(this)->font());
        for (int i = 0; i < all.size(); ++i) {
            const double x = timeToX(all.at(i).t);
            const double right = x + 13.0 + metrics.horizontalAdvance(all.at(i).name);
            if (pos.x() >= x - 3.0 && pos.x() <= right)
                return i;
        }
        return -1;
    }

    EditorCore* m_core = nullptr;        // 不拥有
    QScrollArea* m_scrollOwner = nullptr; // 不拥有（冻结名列用其滚动偏移）

    QList<Row> m_rows;
    QSet<QString> m_collapsedGroups; // "actor"/"screen"，随重建保持（成员变量天然保留）
    double m_pxPerSec = 20.0;        // 默认 60s → 1200px，默认宽度内整体可见

    // ---- 画布统一选择模型状态（画布局部，与对象 SelectionModel 区分）----
    CanvasSel m_sel = CanvasSel::None;
    QList<QPair<QString, double>> m_selectedKeys; // (对象 id, 时刻)，Keys
    int m_selectedLabel = -1;                     // 标签下标，Label
    QString m_selectedTrackId;                    // Track（与对象单选联动）

    // 游标拖动 / 关键帧拖动 / 标签拖动状态
    bool m_scrubbing = false;
    bool m_keyDragArmed = false;  // 按下待命，越过阈值转 m_keyDragging
    bool m_keyDragging = false;
    QString m_dragId;
    double m_dragOldT = 0.0;
    double m_dragGhostT = 0.0;
    bool m_labelDragArmed = false; // 同上，标签拖动
    bool m_labelDragging = false;
    int m_labelDragIndex = -1;
    double m_labelGhostT = 0.0;
    QPoint m_pressPos;
};

// ---- TimelineWidget ----

TimelineWidget::TimelineWidget(EditorCore* editorCore, QWidget* parent)
    : QWidget(parent)
    , m_editorCore(editorCore)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    // ---- 顶部工具行：打帧 / 播放暂停 / 倍速 / 时刻读数 / 加标签 ----
    auto* toolbar = new QHBoxLayout(this);
    m_keyframeButton = new QPushButton(QStringLiteral("打帧"), this);
    m_keyframeButton->setObjectName(QStringLiteral("keyframeButton"));
    m_keyframeButton->setToolTip(
        QStringLiteral("在当前时刻以当前位置+朝向生成/更新关键帧（不弹窗）"));
    m_playButton = new QPushButton(QStringLiteral("播放"), this);
    m_playButton->setObjectName(QStringLiteral("playButton"));
    m_speedCombo = new QComboBox(this);
    m_speedCombo->setObjectName(QStringLiteral("speedCombo"));
    for (const auto& entry : { QPair<QString, double>(QStringLiteral("0.25×"), 0.25),
                               QPair<QString, double>(QStringLiteral("0.5×"), 0.5),
                               QPair<QString, double>(QStringLiteral("1×"), 1.0),
                               QPair<QString, double>(QStringLiteral("2×"), 2.0),
                               QPair<QString, double>(QStringLiteral("4×"), 4.0) })
        m_speedCombo->addItem(entry.first, entry.second);
    m_speedCombo->setCurrentIndex(2); // 1×
    m_timeReadout = new QLabel(this);
    m_timeReadout->setObjectName(QStringLiteral("timeReadout"));
    m_addLabelButton = new QPushButton(QStringLiteral("＋标签"), this);
    m_addLabelButton->setObjectName(QStringLiteral("addLabelButton"));
    m_addLabelButton->setToolTip(QStringLiteral("在当前时刻添加场次标签（纯视觉标记）"));
    toolbar->addWidget(m_keyframeButton);
    toolbar->addWidget(m_playButton);
    toolbar->addWidget(m_speedCombo);
    toolbar->addWidget(m_timeReadout);
    toolbar->addStretch();
    toolbar->addWidget(m_addLabelButton);
    layout->addLayout(toolbar);

    // ---- 画布（横向滚动区） ----
    auto* scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(false);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_canvas = new TimelineCanvas(m_editorCore, scrollArea);
    m_canvas->setObjectName(QStringLiteral("timelineCanvas")); // 供自动化测试定位
    scrollArea->setWidget(m_canvas);
    layout->addWidget(scrollArea);
    // 冻结名列按滚动偏移钉住：滚动区用像素 blit 优化，滚动后必须整幅重绘，
    // 否则 blit 区域内的名列会错位
    connect(scrollArea->horizontalScrollBar(), &QScrollBar::valueChanged,
            m_canvas, [this](int) { m_canvas->update(); });

    TimelinePlayer* player = m_editorCore->timelinePlayer();
    RuntimeData* runtime = m_editorCore->runtimeData();

    connect(m_keyframeButton, &QPushButton::clicked,
            m_editorCore, &EditorCore::keyframeSelectionAtCursor);
    connect(m_playButton, &QPushButton::clicked, this, [player] {
        if (player->isPlaying())
            player->pause();
        else
            player->play();
    });
    connect(m_speedCombo, &QComboBox::activated, this, [this, player](int index) {
        player->setSpeed(m_speedCombo->itemData(index).toDouble());
    });
    connect(m_addLabelButton, &QPushButton::clicked,
            this, &TimelineWidget::addLabelAtCursor);

    connect(runtime, &RuntimeData::fileDataChanged,
            m_canvas, &TimelineCanvas::rebuildRows);
    connect(runtime, &RuntimeData::fileDataChanged,
            this, &TimelineWidget::updateTransport);
    connect(player, &TimelinePlayer::currentTimeChanged,
            m_canvas, [this](double) { m_canvas->update(); });
    connect(player, &TimelinePlayer::currentTimeChanged,
            this, [this](double) { updateTransport(); });
    connect(player, &TimelinePlayer::playingChanged,
            this, [this](bool) { updateTransport(); });
    connect(player, &TimelinePlayer::speedChanged, this, [this](double speed) {
        const int index = m_speedCombo->findData(speed);
        if (index >= 0)
            m_speedCombo->setCurrentIndex(index);
    });
    connect(m_editorCore->selection(), &SelectionModel::selectionChanged,
            this, &TimelineWidget::updateTransport);
    connect(m_editorCore->selection(), &SelectionModel::selectionChanged,
            m_canvas, [this] { m_canvas->syncTrackSelection(); }); // 轨道选中联动 + 名列底色刷新

    updateTransport();
}

double TimelineWidget::pxPerSec() const
{
    return m_canvas->pxPerSec();
}

QPoint TimelineWidget::canvasPosFor(const QString& objectId, double t) const
{
    const int y = m_canvas->trackRowCenterY(objectId);
    if (y < 0)
        return QPoint(-1, -1);
    return QPoint(int(m_canvas->timeToX(t)), y);
}

void TimelineWidget::updateTransport()
{
    TimelinePlayer* player = m_editorCore->timelinePlayer();
    m_playButton->setText(player->isPlaying() ? QStringLiteral("暂停")
                                              : QStringLiteral("播放"));
    m_timeReadout->setText(QStringLiteral("t = %1 s / %2 s")
                               .arg(player->currentTime(), 0, 'f', 2)
                               .arg(player->duration(), 0, 'f', 1));
    // 打帧按钮可用性：单选演员/屏风（多选/空选/空对象禁用）
    m_keyframeButton->setEnabled(m_editorCore->canKeyframeSelection());
}

void TimelineWidget::addLabelAtCursor()
{
    // 自动命名 第N幕（与现有标签去重递增）
    const QList<SceneLabel>& labels =
        m_editorCore->runtimeData()->fileData().timeline.labels;
    int n = labels.size() + 1;
    QString name;
    for (;;) {
        name = QStringLiteral("第%1幕").arg(n);
        bool taken = false;
        for (const SceneLabel& label : labels)
            taken = taken || label.name == name;
        if (!taken)
            break;
        ++n;
    }
    m_editorCore->addSceneLabel(m_editorCore->timelinePlayer()->currentTime(), name);
}
