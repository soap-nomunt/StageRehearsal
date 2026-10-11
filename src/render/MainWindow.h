#pragma once

#include <QList>
#include <QMainWindow>

class QAction;
class QCloseEvent;
class QDockWidget;
class EditorCore;
class HierarchyPanel;
class InspectorPanel;
class RuntimeData;
class SettingsWindow;
class StageScene;
class StageView;
class TimelineWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

    // 验收辅助（正常启动不触发）：截取主视口（默认 1:50 与适配舞台各一张）后退出
    void dumpScreenshotForSelfTest(const QString& path);

    // ---- 文件流程（阶段6）：路径明确的可自测核心，菜单动作只是取路径的薄壳 ----
    enum class UnsavedChoice { Save, Discard, Cancel };

    bool saveToPath(const QString& path);   // 保存到指定路径；失败弹错误提示
    bool openFromPath(const QString& path); // 从指定路径打开；失败弹"文件损坏或无法打开"
    // dirty 时弹三选一（保存/不保存/取消）；不 dirty 直接返回 Discard 语义继续
    UnsavedChoice askUnsaved();
    // 未保存门卫（退出/新建/打开三入口复用）：可以继续返回 true；
    // 选"保存"走保存流程（含无路径转另存为），保存失败或取消返回 false
    bool maybeCloseOrContinue();

protected:
    void closeEvent(QCloseEvent* event) override; // 过同一未保存门卫，取消则 ignore()

private:
    void setupDockPanels();
    void setupMenus();
    void updateEditActions(); // 撤销/重做随栈状态；复制/粘贴/删除随选中与剪贴板
    void updateWindowTitle(); // 标题栏：文件名[*] - 应用名（[*] 随 dirty 显示）
    bool saveWithDialog();    // 保存流程：有路径直存；无路径弹另存为对话框取路径
    // 统一设置窗口（阶段7）：菜单栏顶层"设置"项直达（舞台/偏好两个旧入口已并入），
    // 打开定位"舞台"分类
    SettingsWindow* ensureSettingsWindow(); // 懒创建
    void openSettings();

    RuntimeData* m_runtimeData = nullptr;
    EditorCore* m_editorCore = nullptr;
    StageScene* m_scene = nullptr;
    StageView* m_view = nullptr;
    HierarchyPanel* m_hierarchyPanel = nullptr;
    InspectorPanel* m_inspectorPanel = nullptr;
    TimelineWidget* m_timelineWidget = nullptr;
    SettingsWindow* m_settingsWindow = nullptr; // 设置窗口单例（懒创建；关闭只隐藏不销毁）
    QList<QDockWidget*> m_docks;

    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    QAction* m_copyAction = nullptr;
    QAction* m_pasteAction = nullptr;
    QAction* m_deleteAction = nullptr;
};
