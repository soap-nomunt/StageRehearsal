#pragma once

// Inspector 属性面板：单选显示表单（演员：角色名/位置/朝向/颜色；
// 屏风：额外有大小；空对象：只有名称），修改立即生效（数值框提交即走命令 →
// fileDataChanged → 场景重建体现）；多选或无选中时清空表单；重名拒绝并提示。
// 阶段5：位置/朝向字段显示 TimelinePlayer 当前时刻插值结果（显示值与数据值分离，
// 游标移动随 currentTimeChanged 刷新）；提交走需求第 20 条打帧流程。
// 阶段12：路径页（第五页）——双帧态点选白线进入（EditorCore::activePath 非空
// 优先于对象表单）：名称（派生只读 <对象名>_<起>s至<止>s）、连线类型下拉、
// 圆心角行（无路径点 1 行 / 有路径点 N+1 行，结构不变不重建防输入抖动）、
// 选中路径点 t 行（弧长百分比）、只读段时长/弧长显示；一切提交走命令入撤销栈。
// 阶段13：道具页（第六页）——名称/位置/朝向/颜色 + scaleX/scaleY（倍数制，
// 步进 0.1、范围 0.1~100）+ xy 绑定切换（切换不改值；绑定后一方改动另一方
// 同步增减同一增量）+ 可供站立复选框 + 编辑形状按钮（任务2 启用，本轮占位
// 禁用）+ 只读实际尺寸（宽×深 cm，随 scale/shape 刷新）；一切提交走命令。

#include <QWidget>

#include <QList>
#include <QString>

#include "business/EditorCore.h" // SegmentRef

class EditorCore;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;
struct ObjectData;

class InspectorPanel : public QWidget {
    Q_OBJECT
public:
    explicit InspectorPanel(EditorCore* editorCore, QWidget* parent = nullptr);

private:
    void refresh();          // 选中或数据变化时刷新表单
    void commitName();       // 改名（重名/空名拒绝并提示、恢复原名）
    void commitTransform();  // 位置/朝向数值框提交
    void commitSize();       // 屏风大小
    void pickColor();        // 色块按钮 + QColorDialog

    // ---- 阶段12：路径页 ----
    void refreshPathPage();  // 按 activePath 回显（名称/类型/圆心角行/t/段时长弧长）
    void rebuildPathAngleRows(int legCount); // 结构变化才重建圆心角行（防输入抖动）
    void commitPathType(int index);          // 连线下拉：直线/弧线
    void commitPathPointT();                 // 选中路径点 t 提交（滑动重定位）
    // ---- 阶段13：道具页 ----
    void commitPropScale();    // scaleX/scaleY 提交（绑定时另一方同步增减同一增量）
    void commitPropStandable(); // 可供站立开关提交
    double propShapeWidth(const ObjectData& object) const;  // 未缩放形状包围盒宽
    double propShapeDepth(const ObjectData& object) const;  // 未缩放形状包围盒深

    const ObjectData* currentObject() const; // 当前单选对象，否则空
    void updateColorButton(QPushButton* button, const QColor& color) const;

    EditorCore* m_editorCore = nullptr; // 不拥有

    QStackedWidget* m_stack = nullptr;
    QLabel* m_emptyLabel = nullptr;     // 多选/无选中占位页

    // 演员表单
    QLineEdit* m_actorName = nullptr;
    QDoubleSpinBox* m_actorX = nullptr;
    QDoubleSpinBox* m_actorY = nullptr;
    QDoubleSpinBox* m_actorRotation = nullptr;
    QPushButton* m_actorColor = nullptr;

    // 屏风表单
    QLineEdit* m_screenName = nullptr;
    QDoubleSpinBox* m_screenX = nullptr;
    QDoubleSpinBox* m_screenY = nullptr;
    QDoubleSpinBox* m_screenRotation = nullptr;
    QDoubleSpinBox* m_screenW = nullptr;
    QDoubleSpinBox* m_screenH = nullptr;
    QPushButton* m_screenColor = nullptr;

    // 空对象表单（纯组织节点：只有名称）
    QLineEdit* m_groupName = nullptr;

    // 路径页（阶段12）
    QWidget* m_pathPage = nullptr;
    QLineEdit* m_pathName = nullptr;          // pathNameEdit（派生只读）
    QComboBox* m_pathType = nullptr;          // pathTypeCombo（直线/弧线）
    QList<QLabel*> m_pathAngleLabels;         // 圆心角行标签（结构重建时换）
    QList<QDoubleSpinBox*> m_pathAngleSpins;  // pathLegAngleSpin%1（逐段）
    QWidget* m_pathPointTRow = nullptr;       // 选中路径点 t 行（无选中点隐藏）
    QDoubleSpinBox* m_pathPointT = nullptr;   // pathPointTSpin（弧长百分比 0~1）
    QLabel* m_pathMetrics = nullptr;          // pathMetricsLabel（段时长/弧长只读）
    QVBoxLayout* m_pathAnglesLayout = nullptr; // 圆心角行容器（结构重建时清空重排）
    SegmentRef m_pathAngleSegment;            // 圆心角行结构对应的段（变了才重建）
    bool m_pathAngleSegmentValid = false;
    int m_pathAngleLegCount = -1;

    // 道具页（阶段13）
    QWidget* m_propPage = nullptr;
    QLineEdit* m_propName = nullptr;              // propNameEdit
    QDoubleSpinBox* m_propX = nullptr;            // propXSpin
    QDoubleSpinBox* m_propY = nullptr;            // propYSpin
    QDoubleSpinBox* m_propRotation = nullptr;     // propRotSpin
    QPushButton* m_propColor = nullptr;           // propColorButton
    QDoubleSpinBox* m_propScaleX = nullptr;       // propScaleXSpin
    QDoubleSpinBox* m_propScaleY = nullptr;       // propScaleYSpin
    QPushButton* m_propScaleLink = nullptr;       // propScaleLinkButton（checkable 绑定切换）
    QCheckBox* m_propStandable = nullptr;         // propStandableCheck
    QPushButton* m_propEditShape = nullptr;       // propEditShapeButton（本轮占位禁用）
    QLabel* m_propActualSize = nullptr;           // propActualSizeLabel（只读实际尺寸）

    bool m_updating = false; // 刷新表单期间屏蔽提交槽，防信号回环
};
