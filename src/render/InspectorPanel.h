#pragma once

// Inspector 属性面板：单选显示表单（演员：角色名/位置/朝向/颜色；
// 屏风：额外有大小；空对象：只有名称），修改立即生效（数值框提交即走命令 →
// fileDataChanged → 场景重建体现）；多选或无选中时清空表单；重名拒绝并提示。
// 阶段5：位置/朝向字段显示 TimelinePlayer 当前时刻插值结果（显示值与数据值分离，
// 游标移动随 currentTimeChanged 刷新）；提交走需求第 20 条打帧流程。

#include <QWidget>

class EditorCore;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QStackedWidget;
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

    bool m_updating = false; // 刷新表单期间屏蔽提交槽，防信号回环
};
