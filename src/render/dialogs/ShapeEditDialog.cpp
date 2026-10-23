#include "ShapeEditDialog.h"

#include "business/PathGeometry.h"

#include <QApplication>
#include <QContextMenuEvent>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineF>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QVBoxLayout>

#include <cmath>

namespace {

constexpr double kSnapGridCm = 50.0;   // 拖拽/加点的网格吸附步长（同准备区画布）
constexpr double kMarginPx = 32.0;     // 适配缩放后的四周留边（像素）
constexpr double kVertexHitPx = 8.0;   // 顶点命中半径（像素）
constexpr double kEdgeHitPx = 6.0;     // 边命中容差（像素）

QPointF snappedToGrid(const QPointF& posCm)
{
    return QPointF(std::round(posCm.x() / kSnapGridCm) * kSnapGridCm,
                   std::round(posCm.y() / kSnapGridCm) * kSnapGridCm);
}

} // namespace

// ---- ShapeCanvas：形状编辑画布（哑控件：数据进 setContent、信号出）----
// 本地持有编辑中的 PropShape；渲染与 PropItem 同一采样实现（PathGeometry）。
class ShapeCanvas : public QWidget {
    Q_OBJECT
public:
    explicit ShapeCanvas(QWidget* parent = nullptr) : QWidget(parent)
    {
        setObjectName(QStringLiteral("shapeCanvas"));
        setMinimumSize(420, 320);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        setFocusPolicy(Qt::StrongFocus); // Delete 删点需要键盘焦点
    }

    void setContent(const PropShape& shape)
    {
        m_shape = shape;
        m_selectedVertex = -1;
        m_selectedEdge = -1;
        updateTransform();
        update();
    }

    const PropShape& shape() const { return m_shape; }
    int selectedVertex() const { return m_selectedVertex; }
    int selectedEdge() const { return m_selectedEdge; }

    // 自测钩子：顶点/边中点（采样折线）在画布上的像素坐标
    QPoint vertexCanvasPos(int index) const;
    QPoint edgeCanvasPos(int index) const;

    void setEdgeAngle(int edge, double angleDeg)
    {
        if (edge < 0 || edge >= m_shape.edgeAngles.size())
            return;
        m_shape.edgeAngles[edge] = PathGeometry::clampLegAngle(angleDeg);
        emit shapeEdited();
        update();
    }

    // Delete 删选中顶点（下限 3 顶点：删除后剩 ≥3 才允许；圆模板 2 顶点不可删）
    void deleteSelectedVertex()
    {
        if (m_selectedVertex < 0 || m_shape.vertices.size() <= 3)
            return;
        removeVertex(m_selectedVertex);
    }

signals:
    void shapeEdited();      // 顶点拖拽/加点/删点/改角度后（本地状态已变）
    void selectionChanged(); // 选中顶点/边变化

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void updateTransform();
    void clampPanOffset(); // 平移限界：内容始终与控件相交（同准备区画布）
    QPointF sceneFromCanvas(const QPointF& canvasPos) const;
    QPointF canvasFromScene(const QPointF& scenePos) const;
    QList<QPointF> sampledOutline() const; // 当前形状采样折线（含弧边）
    int vertexAt(const QPoint& canvasPos) const; // 命中顶点下标，无则 -1
    int edgeAt(const QPoint& canvasPos) const;   // 命中边下标（采样距离），无则 -1
    void removeVertex(int index);                // 删点 + 相邻边圆心角合并

    PropShape m_shape;
    int m_selectedVertex = -1;
    int m_selectedEdge = -1;

    QRectF m_contentBBox;    // 内容包围盒（局部坐标 cm）
    double m_scale = 1.0;    // cm → 画布像素
    QPointF m_origin;        // 局部原点在画布上的像素位置（适配结果）
    QPointF m_panOffset;     // 右键平移像素偏移（叠加在适配之上）

    // 右键平移（同准备区画布/主视口语义）
    bool m_panning = false;
    bool m_panMoved = false;
    QPoint m_panPressPos;
    QPoint m_lastPanPos;

    // 顶点拖拽
    bool m_dragging = false; // 已越过阈值
    QPoint m_pressPos;
};

QPoint ShapeCanvas::vertexCanvasPos(int index) const
{
    if (index < 0 || index >= m_shape.vertices.size())
        return QPoint(-1, -1);
    return canvasFromScene(QPointF(m_shape.vertices.at(index).x,
                                   m_shape.vertices.at(index).y))
        .toPoint();
}

QPoint ShapeCanvas::edgeCanvasPos(int index) const
{
    if (index < 0 || index >= m_shape.vertices.size())
        return QPoint(-1, -1);
    const int count = m_shape.vertices.size();
    const PathPoint& a = m_shape.vertices.at(index);
    const PathPoint& b = m_shape.vertices.at((index + 1) % count);
    const double angle = index < m_shape.edgeAngles.size() ? m_shape.edgeAngles.at(index) : 0.0;
    const QList<QPointF> leg = PathGeometry::sampleLeg(QPointF(a.x, a.y),
                                                       QPointF(b.x, b.y), angle);
    // 弧长中点（直边 leg 仅两点，取中间采样点会落到端点=顶点上）
    return canvasFromScene(
                   PathGeometry::pointAtRatio(leg, PathGeometry::cumulativeLengths(leg), 0.5))
        .toPoint();
}

void ShapeCanvas::updateTransform()
{
    const QList<QPointF> outline = sampledOutline();
    // 注意：QRectF(p,p) 是零尺寸矩形（isNull），不能直接 united 累积——手动求包围盒
    QRectF bbox;
    bool first = true;
    double minX = 0.0, minY = 0.0, maxX = 0.0, maxY = 0.0;
    for (const QPointF& p : outline) {
        if (first) {
            minX = maxX = p.x();
            minY = maxY = p.y();
            first = false;
        } else {
            minX = qMin(minX, p.x());
            minY = qMin(minY, p.y());
            maxX = qMax(maxX, p.x());
            maxY = qMax(maxY, p.y());
        }
    }
    bbox = first ? QRectF(-1.0, -1.0, 2.0, 2.0)
                 : QRectF(QPointF(minX, minY), QPointF(maxX, maxY));
    if (bbox.isEmpty())
        bbox = QRectF(-1.0, -1.0, 2.0, 2.0);
    m_contentBBox = bbox;

    const double availW = qMax(1.0, width() - 2.0 * kMarginPx);
    const double availH = qMax(1.0, height() - 2.0 * kMarginPx);
    m_scale = qMin(availW / bbox.width(), availH / bbox.height());
    m_origin = QPointF((width() - bbox.width() * m_scale) / 2.0 - bbox.left() * m_scale,
                       (height() - bbox.height() * m_scale) / 2.0 - bbox.top() * m_scale);
    clampPanOffset();
}

void ShapeCanvas::clampPanOffset()
{
    if (m_contentBBox.isEmpty())
        return;
    const QRectF px(m_origin.x() + m_panOffset.x() + m_contentBBox.left() * m_scale,
                    m_origin.y() + m_panOffset.y() + m_contentBBox.top() * m_scale,
                    m_contentBBox.width() * m_scale, m_contentBBox.height() * m_scale);
    const QRectF view = QRectF(rect());
    if (px.right() <= view.left())
        m_panOffset.rx() += view.left() - px.right() + 1.0;
    else if (px.left() >= view.right())
        m_panOffset.rx() += view.right() - px.left() - 1.0;
    if (px.bottom() <= view.top())
        m_panOffset.ry() += view.top() - px.bottom() + 1.0;
    else if (px.top() >= view.bottom())
        m_panOffset.ry() += view.bottom() - px.top() - 1.0;
}

QPointF ShapeCanvas::sceneFromCanvas(const QPointF& canvasPos) const
{
    return QPointF((canvasPos.x() - m_origin.x() - m_panOffset.x()) / m_scale,
                   (canvasPos.y() - m_origin.y() - m_panOffset.y()) / m_scale);
}

QPointF ShapeCanvas::canvasFromScene(const QPointF& scenePos) const
{
    return QPointF(m_origin.x() + m_panOffset.x() + scenePos.x() * m_scale,
                   m_origin.y() + m_panOffset.y() + scenePos.y() * m_scale);
}

QList<QPointF> ShapeCanvas::sampledOutline() const
{
    QList<QPointF> outline;
    const int count = m_shape.vertices.size();
    for (int i = 0; i < count; ++i) {
        const PathPoint& a = m_shape.vertices.at(i);
        const PathPoint& b = m_shape.vertices.at((i + 1) % count);
        const double angle = i < m_shape.edgeAngles.size() ? m_shape.edgeAngles.at(i) : 0.0;
        const QList<QPointF> leg =
            PathGeometry::sampleLeg(QPointF(a.x, a.y), QPointF(b.x, b.y), angle);
        for (int j = (outline.isEmpty() ? 0 : 1); j < leg.size(); ++j)
            outline.append(leg.at(j));
    }
    return outline;
}

int ShapeCanvas::vertexAt(const QPoint& canvasPos) const
{
    for (int i = m_shape.vertices.size() - 1; i >= 0; --i) { // 后画的优先命中
        if (QLineF(QPointF(canvasPos), vertexCanvasPos(i)).length() <= kVertexHitPx)
            return i;
    }
    return -1;
}

int ShapeCanvas::edgeAt(const QPoint& canvasPos) const
{
    const int count = m_shape.vertices.size();
    for (int i = 0; i < count; ++i) {
        const PathPoint& a = m_shape.vertices.at(i);
        const PathPoint& b = m_shape.vertices.at((i + 1) % count);
        const double angle = i < m_shape.edgeAngles.size() ? m_shape.edgeAngles.at(i) : 0.0;
        const QList<QPointF> leg =
            PathGeometry::sampleLeg(QPointF(a.x, a.y), QPointF(b.x, b.y), angle);
        for (int j = 0; j + 1 < leg.size(); ++j) {
            const QPointF va = canvasFromScene(leg.at(j));
            const QPointF vb = canvasFromScene(leg.at(j + 1));
            const QPointF ab = vb - va;
            const double len2 = ab.x() * ab.x() + ab.y() * ab.y();
            double f = 0.0;
            if (len2 > 0.0) {
                f = qBound(0.0, ((canvasPos.x() - va.x()) * ab.x()
                                 + (canvasPos.y() - va.y()) * ab.y()) / len2,
                           1.0);
            }
            const QPointF proj = va + ab * f;
            if (std::hypot(canvasPos.x() - proj.x(), canvasPos.y() - proj.y())
                <= kEdgeHitPx)
                return i;
        }
    }
    return -1;
}

void ShapeCanvas::removeVertex(int index)
{
    const int count = m_shape.vertices.size();
    if (index < 0 || index >= count || count <= 3)
        return; // 下限 3 顶点（圆模板 2 顶点同样不可删）
    m_shape.vertices.removeAt(index);
    // 相邻两边合并：新边（v[i-1]→v[i+1]）圆心角 = θ[i-1] + θ[i]（加点拆分逆操作）
    const int prev = (index - 1 + count) % count;
    if (prev < m_shape.edgeAngles.size() && index < m_shape.edgeAngles.size())
        m_shape.edgeAngles[prev] += m_shape.edgeAngles.at(index);
    if (index < m_shape.edgeAngles.size())
        m_shape.edgeAngles.removeAt(index);
    m_selectedVertex = -1;
    m_selectedEdge = -1;
    emit shapeEdited();
    emit selectionChanged();
    updateTransform();
    update();
}

void ShapeCanvas::paintEvent(QPaintEvent* /*event*/)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), QColor(QStringLiteral("#242424"))); // 与主视口同底色

    // 网格（50cm，淡灰）
    QPen gridPen(QColor(QStringLiteral("#3a3a3a")));
    gridPen.setCosmetic(true);
    painter.setPen(gridPen);
    const QPointF gridTopLeft = sceneFromCanvas(QPointF(0.0, 0.0));
    const QPointF gridBottomRight = sceneFromCanvas(QPointF(width(), height()));
    for (double x = std::floor(gridTopLeft.x() / kSnapGridCm) * kSnapGridCm;
         x <= gridBottomRight.x(); x += kSnapGridCm) {
        const double px = canvasFromScene(QPointF(x, 0.0)).x();
        painter.drawLine(QPointF(px, 0.0), QPointF(px, height()));
    }
    for (double y = std::floor(gridTopLeft.y() / kSnapGridCm) * kSnapGridCm;
         y <= gridBottomRight.y(); y += kSnapGridCm) {
        const double py = canvasFromScene(QPointF(0.0, y)).y();
        painter.drawLine(QPointF(0.0, py), QPointF(width(), py));
    }

    // 形状轮廓（采样折线）：选中边加粗高亮
    const QList<QPointF> outline = sampledOutline();
    QPainterPath path;
    if (!outline.isEmpty()) {
        path.moveTo(canvasFromScene(outline.first()));
        for (int i = 1; i < outline.size(); ++i)
            path.lineTo(canvasFromScene(outline.at(i)));
        path.closeSubpath();
    }
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(QStringLiteral("#b0814e66"))); // 淡填充
    painter.drawPath(path);
    QPen outlinePen(QColor(QStringLiteral("#e8e8e8")));
    outlinePen.setWidth(2);
    painter.setPen(outlinePen);
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(path);
    if (m_selectedEdge >= 0 && m_selectedEdge < m_shape.vertices.size()) {
        // 选中边单独加粗亮蓝
        const int count = m_shape.vertices.size();
        const PathPoint& a = m_shape.vertices.at(m_selectedEdge);
        const PathPoint& b = m_shape.vertices.at((m_selectedEdge + 1) % count);
        const double angle = m_selectedEdge < m_shape.edgeAngles.size()
            ? m_shape.edgeAngles.at(m_selectedEdge)
            : 0.0;
        const QList<QPointF> leg =
            PathGeometry::sampleLeg(QPointF(a.x, a.y), QPointF(b.x, b.y), angle);
        QPainterPath legPath;
        legPath.moveTo(canvasFromScene(leg.first()));
        for (int i = 1; i < leg.size(); ++i)
            legPath.lineTo(canvasFromScene(leg.at(i)));
        QPen selPen(QColor(QStringLiteral("#4a90d9")));
        selPen.setWidth(4);
        painter.setPen(selPen);
        painter.drawPath(legPath);
    }

    // 顶点标记：白色小方块（选中蓝色加大）
    for (int i = 0; i < m_shape.vertices.size(); ++i) {
        const QPointF px = canvasFromScene(
            QPointF(m_shape.vertices.at(i).x, m_shape.vertices.at(i).y));
        const bool selected = i == m_selectedVertex;
        const double half = selected ? 6.0 : 4.0;
        painter.setPen(QPen(QColor(QStringLiteral("#202020"))));
        painter.setBrush(selected ? QColor(QStringLiteral("#4a90d9")) : QColor(Qt::white));
        painter.drawRect(QRectF(px.x() - half, px.y() - half, half * 2, half * 2));
    }
}

void ShapeCanvas::resizeEvent(QResizeEvent* event)
{
    updateTransform();
    QWidget::resizeEvent(event);
}

void ShapeCanvas::mousePressEvent(QMouseEvent* event)
{
    setFocus();
    // 手势嵌套防护（同主视口）
    if (m_panning || m_dragging) {
        event->accept();
        return;
    }
    if (event->button() == Qt::RightButton) {
        // 右键平移（同准备区画布/主视口）：任意位置按下即手势，越阈值才移动
        m_panning = true;
        m_panMoved = false;
        m_panPressPos = event->pos();
        m_lastPanPos = m_panPressPos;
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    if (event->button() == Qt::LeftButton) {
        const int vertex = vertexAt(event->pos());
        if (vertex >= 0) {
            if (vertex != m_selectedVertex) {
                m_selectedVertex = vertex;
                m_selectedEdge = -1;
                emit selectionChanged();
            }
            m_dragging = false; // 待判定：越过阈值转拖动
            m_pressPos = event->pos();
            m_selectedVertex = vertex;
            update();
        } else {
            const int edge = edgeAt(event->pos());
            if (edge != m_selectedEdge) {
                m_selectedEdge = edge;
                m_selectedVertex = -1;
                emit selectionChanged();
                update();
            } else if (edge < 0 && (m_selectedVertex >= 0 || m_selectedEdge >= 0)) {
                m_selectedVertex = -1;
                m_selectedEdge = -1;
                emit selectionChanged();
                update();
            }
        }
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ShapeCanvas::mouseMoveEvent(QMouseEvent* event)
{
    if (m_panning) {
        const QPoint pos = event->pos();
        if (!m_panMoved
            && (pos - m_panPressPos).manhattanLength()
                >= QApplication::startDragDistance())
            m_panMoved = true;
        if (m_panMoved) {
            m_panOffset += QPointF(pos - m_lastPanPos);
            clampPanOffset();
            update();
        }
        m_lastPanPos = pos;
        event->accept();
        return;
    }
    if (m_selectedVertex >= 0
        && (event->buttons() & Qt::LeftButton)) {
        if (!m_dragging
            && (event->pos() - m_pressPos).manhattanLength()
                >= QApplication::startDragDistance())
            m_dragging = true;
        if (m_dragging) {
            // 顶点拖拽：本地状态直改（吸附 50cm），编辑期间不动 FileData
            const QPointF snapped = snappedToGrid(sceneFromCanvas(event->pos()));
            m_shape.vertices[m_selectedVertex] = PathPoint { snapped.x(), snapped.y() };
            emit shapeEdited();
            update();
        }
        event->accept();
        return;
    }
    QWidget::mouseMoveEvent(event);
}

void ShapeCanvas::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton && m_panning) {
        m_panning = false;
        unsetCursor();
        event->accept(); // m_panMoved 留给 contextMenuEvent 判定
        return;
    }
    if (event->button() == Qt::LeftButton && m_selectedVertex >= 0) {
        if (m_dragging) {
            m_dragging = false;
            updateTransform(); // 内容包围盒可能变化：适配重算（平移偏移保留）
            update();
        }
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void ShapeCanvas::contextMenuEvent(QContextMenuEvent* event)
{
    if (m_panMoved) {
        // 右键平移后松开的这次：吞掉不弹菜单（同主视口）
        m_panMoved = false;
        event->accept();
        return;
    }
    const int vertex = vertexAt(event->pos());
    if (vertex >= 0) {
        QMenu menu(this);
        QAction* removeAction = menu.addAction(QStringLiteral("删除顶点"));
        removeAction->setEnabled(m_shape.vertices.size() > 3); // 下限 3 顶点
        m_selectedVertex = vertex;
        m_selectedEdge = -1;
        emit selectionChanged();
        update();
        if (menu.exec(event->globalPos()) == removeAction)
            removeVertex(vertex);
        return;
    }
    const int edge = edgeAt(event->pos());
    if (edge >= 0) {
        m_selectedEdge = edge;
        m_selectedVertex = -1;
        emit selectionChanged();
        update();
        QMenu menu(this);
        QAction* addAction = menu.addAction(QStringLiteral("添加顶点"));
        QAction* straightenAction = menu.addAction(QStringLiteral("恢复直线"));
        const bool isArc = edge < m_shape.edgeAngles.size()
            && PathGeometry::legIsArc(m_shape.edgeAngles.at(edge));
        straightenAction->setEnabled(isArc); // 已是直边则禁用
        QAction* chosen = menu.exec(event->globalPos());
        if (chosen == addAction) {
            // 加点：吸附到边采样点（最近点），圆心角按弧长比例拆分保形
            const int count = m_shape.vertices.size();
            const PathPoint& a = m_shape.vertices.at(edge);
            const PathPoint& b = m_shape.vertices.at((edge + 1) % count);
            const double angle =
                edge < m_shape.edgeAngles.size() ? m_shape.edgeAngles.at(edge) : 0.0;
            const QList<QPointF> leg =
                PathGeometry::sampleLeg(QPointF(a.x, a.y), QPointF(b.x, b.y), angle);
            const QPointF scenePos = sceneFromCanvas(event->pos());
            const double ratio = PathGeometry::projectToRatio(leg, scenePos);
            const QPointF onEdge = PathGeometry::pointAtRatio(
                leg, PathGeometry::cumulativeLengths(leg), ratio);
            m_shape.vertices.insert(edge + 1, PathPoint { onEdge.x(), onEdge.y() });
            m_shape.edgeAngles[edge] = ratio * angle;             // θ₁ = f·θ
            m_shape.edgeAngles.insert(edge + 1, (1.0 - ratio) * angle); // θ₂ = (1−f)·θ
            m_selectedEdge = -1;
            m_selectedVertex = edge + 1; // 新点自动选中（便于接着拖）
            emit shapeEdited();
            emit selectionChanged();
            updateTransform();
            update();
        } else if (chosen == straightenAction) {
            setEdgeAngle(edge, 0.0); // 恢复直线（该边圆心角归零）
            emit selectionChanged();
        }
        return;
    }
    // 空白：无菜单
}

void ShapeCanvas::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Delete) {
        deleteSelectedVertex();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

// ---- ShapeEditDialog ----

ShapeEditDialog::ShapeEditDialog(const PropShape& initial, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("编辑形状"));
    setModal(true);
    resize(640, 560);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    // 基础形状生成行：固定比例模板，生成后即按多边形编辑（本地状态，可继续改）
    auto* templateRow = new QHBoxLayout;
    templateRow->addWidget(new QLabel(QStringLiteral("基础形状："), this));
    const auto addTemplate = [this, templateRow](const QString& text,
                                                 const QString& objectName,
                                                 const PropShape& shape) {
        auto* button = new QPushButton(text, this);
        button->setObjectName(objectName);
        connect(button, &QPushButton::clicked, this, [this, shape] {
            m_canvas->setContent(shape);
            revalidate();
            refreshEdgeRow();
        });
        templateRow->addWidget(button);
    };
    addTemplate(QStringLiteral("矩形"), QStringLiteral("templateRectButton"),
                defaultPropShape());
    // 圆形 = 2 顶点 + 两条 180° 弧边（逆时针：右极点→左极点走上半圆）
    addTemplate(QStringLiteral("圆形"), QStringLiteral("templateCircleButton"),
                PropShape { { { 50.0, 0.0 }, { -50.0, 0.0 } }, { -180.0, -180.0 } });
    // 梯形：上底 60 下底 100 高 80（逆时针）
    addTemplate(QStringLiteral("梯形"), QStringLiteral("templateTrapezoidButton"),
                PropShape { { { -30.0, -40.0 }, { -50.0, 40.0 }, { 50.0, 40.0 },
                              { 30.0, -40.0 } },
                            { 0.0, 0.0, 0.0, 0.0 } });
    // 三角形（逆时针）
    addTemplate(QStringLiteral("三角形"), QStringLiteral("templateTriangleButton"),
                PropShape { { { -50.0, 40.0 }, { 50.0, 40.0 }, { 0.0, -60.0 } },
                            { 0.0, 0.0, 0.0 } });
    templateRow->addStretch();
    layout->addLayout(templateRow);

    // 画布
    m_canvas = new ShapeCanvas(this);
    m_canvas->setContent(initial);
    layout->addWidget(m_canvas, 1);

    // 圆心角行（选中边时显示）：规则同路径编辑
    m_edgeRow = new QWidget(this);
    m_edgeRow->setObjectName(QStringLiteral("edgeAngleRow")); // 供自测定位
    auto* edgeLayout = new QHBoxLayout(m_edgeRow);
    edgeLayout->setContentsMargins(0, 0, 0, 0);
    m_edgeRowLabel = new QLabel(m_edgeRow);
    m_edgeRowLabel->setObjectName(QStringLiteral("edgeRowLabel"));
    m_edgeAngleSpin = new QDoubleSpinBox(m_edgeRow);
    m_edgeAngleSpin->setObjectName(QStringLiteral("edgeAngleSpin"));
    m_edgeAngleSpin->setRange(-PathGeometry::kMaxLegAngleDeg, PathGeometry::kMaxLegAngleDeg);
    m_edgeAngleSpin->setDecimals(1);
    m_edgeAngleSpin->setSingleStep(5.0);
    m_edgeAngleSpin->setSuffix(QStringLiteral(" °"));
    m_edgeAngleSpin->setKeyboardTracking(false);
    m_edgeAngleSpin->setToolTip(
        QStringLiteral("圆心角（度，±350；正 = 沿边行进方向左侧鼓出；|θ|<0.1° 按直线）"));
    edgeLayout->addWidget(m_edgeRowLabel);
    edgeLayout->addWidget(m_edgeAngleSpin);
    edgeLayout->addStretch();
    layout->addWidget(m_edgeRow);

    connect(m_edgeAngleSpin, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        m_canvas->setEdgeAngle(m_canvas->selectedEdge(), value); // 本地状态直改
        revalidate();
    });

    // 校验状态行 + 确定/取消
    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName(QStringLiteral("shapeStatusLabel"));
    layout->addWidget(m_statusLabel);

    auto* buttonRow = new QHBoxLayout;
    buttonRow->addStretch();
    m_okButton = new QPushButton(QStringLiteral("确定"), this);
    m_okButton->setObjectName(QStringLiteral("shapeOkButton"));
    auto* cancelButton = new QPushButton(QStringLiteral("取消"), this);
    cancelButton->setObjectName(QStringLiteral("shapeCancelButton")); // 供自测定位
    connect(m_okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonRow->addWidget(m_okButton);
    buttonRow->addWidget(cancelButton);
    layout->addLayout(buttonRow);

    connect(m_canvas, &ShapeCanvas::shapeEdited, this, [this] {
        revalidate();
        refreshEdgeRow(); // 加删点后选中边/行回显跟随
    });
    connect(m_canvas, &ShapeCanvas::selectionChanged, this, [this] { refreshEdgeRow(); });

    revalidate();
    refreshEdgeRow();
}
PropShape ShapeEditDialog::resultShape() const
{
    return m_canvas->shape();
}

PropShape ShapeEditDialog::currentShape() const
{
    return m_canvas->shape();
}

QPoint ShapeEditDialog::canvasVertexPos(int index) const
{
    return m_canvas->vertexCanvasPos(index);
}

QPoint ShapeEditDialog::canvasEdgePos(int index) const
{
    return m_canvas->edgeCanvasPos(index);
}

void ShapeEditDialog::revalidate()
{
    const PathGeometry::ShapeValidity result =
        PathGeometry::validatePropShape(m_canvas->shape());
    m_statusLabel->setText(result.valid ? QStringLiteral("形状合法")
                                        : result.message);
    m_statusLabel->setStyleSheet(
        result.valid ? QStringLiteral("color: #888888;")
                     : QStringLiteral("color: #e74c3c;")); // 非法红字
    m_okButton->setEnabled(result.valid); // 非法禁止结束编辑
}

void ShapeEditDialog::refreshEdgeRow()
{
    const int edge = m_canvas->selectedEdge();
    if (edge < 0 || edge >= m_canvas->shape().edgeAngles.size()) {
        m_edgeRow->setVisible(false);
        return;
    }
    m_edgeRow->setVisible(true);
    const int count = m_canvas->shape().vertices.size();
    m_edgeRowLabel->setText(QStringLiteral("边 %1（v%2 → v%3）圆心角")
                                .arg(edge + 1)
                                .arg(edge + 1)
                                .arg((edge + 1) % count + 1));
    const QSignalBlocker blocker(m_edgeAngleSpin); // 回显不触发提交
    m_edgeAngleSpin->setValue(m_canvas->shape().edgeAngles.at(edge));
}

// ShapeCanvas 定义在本文件（.cpp 内的 Q_OBJECT 类需要 moc 生成文件）
#include "ShapeEditDialog.moc"
