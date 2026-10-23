#pragma once

// 道具形状编辑器（阶段13 任务2，模态 QDialog）：编辑本地副本，确定时由调用方
// 经 EditorCore::setPropShape 一条命令入撤销栈（取消零变更）。
// 布局：顶部基础形状生成行（矩形/圆形/梯形/三角形，固定比例模板，生成后即按
// 多边形编辑）+ 中部画布（ShapeCanvas）+ 底部（选中边圆心角行 / 校验状态行 /
// 确定取消）。
// 画布交互（规则同路径编辑）：左键拖顶点（50cm 吸附）/ 点选边改圆心角 /
// 右键边上加点（吸附到边采样点 + 圆心角按弧长比例拆分保形）/ 右键顶点删除
// （下限 3 顶点）/ Delete 删选中顶点 / 右键拖拽平移（平移过不弹菜单）；
// 合围实时校验（PathGeometry::validatePropShape 纯函数）：非法 → 状态行红字
// （指明首处原因）+ 确定禁用；编辑过程允许中间非法态。

#include "data/DataTypes.h"

#include <QDialog>

class QLabel;
class QDoubleSpinBox;
class QPushButton;
class ShapeCanvas;

class ShapeEditDialog : public QDialog {
    Q_OBJECT
public:
    // initial：进入时的形状（调用方传道具当前 shape，缺省矩形由调用方归一）
    explicit ShapeEditDialog(const PropShape& initial, QWidget* parent = nullptr);

    PropShape resultShape() const; // accept 后读（= 画布最终形状）

    // 自测钩子：编辑中的本地形状 / 顶点·边中点的画布像素坐标（越界 (-1,-1)）
    PropShape currentShape() const;
    QPoint canvasVertexPos(int index) const;
    QPoint canvasEdgePos(int index) const;

private:
    void revalidate();      // 实时合围校验 → 状态行 + 确定可用性
    void refreshEdgeRow();  // 圆心角行随选中边显隐与回显

    ShapeCanvas* m_canvas = nullptr;
    QDoubleSpinBox* m_edgeAngleSpin = nullptr; // edgeAngleSpin
    QLabel* m_edgeRowLabel = nullptr;          // edgeRowLabel（"边 N（v→w）圆心角"）
    QWidget* m_edgeRow = nullptr;              // edgeAngleRow（无选中边隐藏）
    QLabel* m_statusLabel = nullptr;           // shapeStatusLabel
    QPushButton* m_okButton = nullptr;         // shapeOkButton
};
