#pragma once

// 时间轴面板（阶段5）：顶部工具行（打帧 / 播放暂停 / 倍速 / 时刻读数 / 加标签）
// + 画布（秒标尺、场次标签行、轨道区——每对象一行、同类可折叠、关键帧点、
// 可拖动游标）。关键帧/场次标签/轨道编辑全部走 EditorCore 命令入撤销栈。
// 轨道随 fileDataChanged 重建，同类折叠状态随重建保持（成员 QSet，参照
// Hierarchy 做法）。横向滚动经 QScrollArea；Ctrl+滚轮缩放时间轴（pxPerSec
// 2~200）。左侧名列冻结（绘制时按滚动偏移钉住，不随横向滚动移出）。
//
// 画布统一选择模型（画布局部状态，不入存档）：游标 / 关键帧（Ctrl 追加多选）/
// 场次标签 / 轨道四类可选中，同一时刻只有一类非空；轨道选中与对象
// SelectionModel 联动（点轨道单选对象、外部单选有轨道对象同步选中轨道）。
// 快捷键仲裁（ShortcutOverride）：Delete/Ctrl+C/Ctrl+V 只在画布选择非空
// （关键帧/标签）或关键帧剪贴板非空时归画布，否则落回编辑菜单对象语义；
// ←/→ 按画布选择类型分发（游标/关键帧/标签 ±0.5s 步进，与游标拖拽吸附一致）。

#include <QWidget>

class EditorCore;
class QComboBox;
class QLabel;
class QPushButton;
class TimelineCanvas;

class TimelineWidget : public QWidget {
    Q_OBJECT
public:
    explicit TimelineWidget(EditorCore* editorCore, QWidget* parent = nullptr);

    // 布局常量（名列宽/标尺高/标签行高；供自测换算坐标）
    static constexpr int kNameColW = 140;
    static constexpr int kRulerH = 22;
    static constexpr int kLabelRowH = 20;

    double pxPerSec() const;

    // 供自测：对象轨道上时刻 t 处的画布坐标（无轨道返回 (-1,-1)）
    QPoint canvasPosFor(const QString& objectId, double t) const;

private:
    void updateTransport(); // 播放按钮文案 / 时刻读数 / 打帧按钮可用性
    void addLabelAtCursor(); // “＋标签”：游标处添加，自动命名 第N幕（去重）

    EditorCore* m_editorCore = nullptr; // 不拥有

    QPushButton* m_keyframeButton = nullptr;
    QPushButton* m_playButton = nullptr;
    QComboBox* m_speedCombo = nullptr;
    QLabel* m_timeReadout = nullptr;
    QPushButton* m_addLabelButton = nullptr;
    TimelineCanvas* m_canvas = nullptr;
};
