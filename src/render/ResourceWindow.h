#pragma once

// 项目资源库窗口（阶段14）：与设置窗口一致的非模态单例形式——主窗口懒创建
// 并持有，关闭只隐藏；"窗口"菜单打开时 show/raise/activate。列表单选并按导入
// 顺序显示类型图标 + 别名；选中资源同步 EditorCore::activeResourceId，由主窗口
// 现有 Inspector 显示资源属性/预览（本窗口不生成缩略图网格）。
//
// 导入：若当前项目无路径先经保存门卫（MainWindow 提供回调），取消/失败则中止；
// 多文件混合导入的成功项合并为一条命令，失败项汇总提示。移除只删清单项，
// 不删除磁盘文件。资源库内 Delete 键不串联主窗口对象删除。

#include <QDialog>

#include <QSet>
#include <QString>

#include <functional>

class EditorCore;
class QLabel;
class QListWidget;
class QPushButton;
class QStackedWidget;

class ResourceWindow : public QDialog {
    Q_OBJECT
public:
    using SaveGate = std::function<bool()>;

    ResourceWindow(EditorCore* editorCore, SaveGate ensureSaved, QWidget* parent = nullptr);

    void openWindow(); // show/raise/activate 复用已显示窗口

protected:
    void closeEvent(QCloseEvent* event) override; // 关闭只隐藏

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
