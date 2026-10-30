#pragma once

// 项目资源库面板（阶段14）：默认位于 Hierarchy 下方、时间轴左侧，与 Hierarchy
// 等宽、与时间轴等高，由主窗口 QDockWidget 持有。列表单选并按导入顺序显示类型
// 图标 + 别名；选中资源同步 EditorCore::activeResourceId，由主窗口现有 Inspector
// 显示资源属性/预览（本面板不生成缩略图网格）。
//
// 导入：若当前项目无路径先经保存门卫（MainWindow 提供回调），取消/失败则中止；
// 多文件混合导入的成功项合并为一条命令，失败项汇总提示。移除只删清单项，
// 不删除磁盘文件。资源库内 Delete 键不串联主窗口对象删除。

#include <QWidget>

#include <QSet>
#include <QString>

#include <functional>

class EditorCore;
class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;

class ResourceWindow : public QWidget {
    Q_OBJECT
public:
    using SaveGate = std::function<bool()>;

    ResourceWindow(EditorCore* editorCore, SaveGate ensureSaved, QWidget* parent = nullptr);

private:
    void refreshList();       // fileDataChanged / 打开项目后重建列表
    void syncSelection();     // activeResourceId → 列表选中行
    void importResources();   // 保存门卫 → 文件对话框 → 分类复制 → 批量命令
    void removeActive();      // 移除选中资源（不删磁盘文件）

    EditorCore* m_editorCore = nullptr; // 不拥有
    SaveGate m_ensureSaved;

    QPushButton* m_importButton = nullptr;
    QPushButton* m_removeButton = nullptr;
    QStackedWidget* m_stack = nullptr;
    QListWidget* m_list = nullptr;
    QLabel* m_emptyLabel = nullptr;
    bool m_updating = false;
};
