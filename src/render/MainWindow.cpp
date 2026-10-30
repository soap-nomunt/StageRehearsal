#include "MainWindow.h"

#include "HierarchyPanel.h"
#include "InspectorPanel.h"
#include "ResourceWindow.h"
#include "SettingsWindow.h"
#include "StageScene.h"
#include "StageView.h"
#include "StartDialog.h"
#include "TimelineWidget.h"
#include "business/EditorCore.h"
#include "business/SelectionModel.h"
#include "data/AppSettings.h"
#include "data/ResourceFiles.h"
#include "data/RuntimeData.h"

#include <QAction>
#include <QApplication>
#include <QAbstractButton>
#include <QCloseEvent>
#include <QCoreApplication>
#include <QDockWidget>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QTimer>

namespace {

const char kArchiveFilter[] = "舞台剧存档 (*.json)"; // 文件对话框过滤器

bool sameArchivePath(const QString& a, const QString& b)
{
    const QString left = QFileInfo(a).absoluteFilePath();
    const QString right = QFileInfo(b).absoluteFilePath();
#if defined(Q_OS_WIN)
    return left.compare(right, Qt::CaseInsensitive) == 0;
#else
    return left == right;
#endif
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    m_runtimeData = new RuntimeData(this);
    m_editorCore = new EditorCore(m_runtimeData, this);
    m_scene = new StageScene(m_runtimeData, m_editorCore->selection(),
                             m_editorCore->timelinePlayer(), this);
    m_view = new StageView(m_scene, m_editorCore, this);
    setCentralWidget(m_view);

    setupDockPanels();
    setupMenus();

    // 标题栏随 dirty/路径刷新：dirty 翻转、文件替换、保存成功（路径变化）都覆盖到
    connect(m_runtimeData, &RuntimeData::dirtyChanged,
            this, &MainWindow::updateWindowTitle);
    connect(m_runtimeData, &RuntimeData::fileReplaced,
            this, &MainWindow::updateWindowTitle);
    connect(m_runtimeData, &RuntimeData::fileDataChanged,
            this, &MainWindow::updateWindowTitle);
    // 阶段12：双帧段选中态 → 主视口幽灵/路径可视化；选中路径 → 路径点标记
    connect(m_editorCore, &EditorCore::activeSegmentChanged, m_scene, [this] {
        m_scene->setActiveSegment(m_editorCore->activeSegment());
    });
    connect(m_editorCore, &EditorCore::activePathChanged, m_scene, [this] {
        m_scene->setActivePath(m_editorCore->activePath());
    });

    // 阶段8：构造函数不再加载任何文件（构造即默认空舞台，无文件 IO）；
    // 启动时的存档选择由启动页（runStartDialogLoop）决定，截图自测模式直开示例存档
    updateWindowTitle();
    m_view->resetView();

    // 窗口布局持久化（阶段9）：先给首次运行默认尺寸；有存档则恢复
    // （空 = 首次运行，保持默认；restore 失败静默退回默认，不弹错误）。
    // 接线点必须在末尾：restoreState 依赖 setupDockPanels 已建好带 objectName 的 Dock
    resize(1440, 900);
    AppSettings settings;
    if (!settings.windowGeometry().isEmpty())
        restoreGeometry(settings.windowGeometry());
    const QByteArray savedState = settings.windowState();
    if (!savedState.isEmpty()) {
        restoreState(savedState);
        if (!savedState.contains("resourceDock"))
            applyDefaultResourceLayout();
    }
}

void MainWindow::dumpScreenshotForSelfTest(const QString& path)
{
    // 等窗口完成首次布局/绘制后：先按默认 1:50 截一张，再适配内容包围盒截一张
    QTimer::singleShot(400, this, [this, path]() {
        QString defaultPath = path;
        defaultPath.replace(QStringLiteral(".png"), QStringLiteral("-1to50.png"));
        m_view->viewport()->grab().save(defaultPath);

        // 阶段12：示例存档含移动路径时，把双帧选中态设到第一条路径的段上再截
        // （基准图含幽灵 + 弧线路径）；无路径则保持原样
        if (!m_runtimeData->fileData().paths.isEmpty()) {
            const PathData& firstPath = m_runtimeData->fileData().paths.first();
            m_editorCore->setActiveSegment(
                SegmentRef { firstPath.objectId, firstPath.from, firstPath.to });
        }

        // 适配内容包围盒（舞台 ∪ 准备区，阶段10），准备区进入基准图
        const QRectF content = m_scene->contentRect().adjusted(-80.0, -80.0, 80.0, 80.0);
        m_view->fitInView(content, Qt::KeepAspectRatio);
        m_view->viewport()->grab().save(path);

        // 整窗一张：含时间轴 Dock（阶段5 起），供人工复核轨道/游标/标签渲染
        QString windowPath = path;
        windowPath.replace(QStringLiteral(".png"), QStringLiteral("-window.png"));
        grab().save(windowPath);

        // 阶段7：再截统一设置窗口（"舞台"分类页），供人工复核 VSCode 风格布局；
        // 设置窗口只读取/展示数据，打开它不改文件与设置
        openSettings();
        QTimer::singleShot(200, this, [this, path]() {
            QString settingsPath = path;
            settingsPath.replace(QStringLiteral(".png"), QStringLiteral("-settings.png"));
            m_settingsWindow->grab().save(settingsPath);
            QCoreApplication::quit();
        });
    });
}

// ---- 文件流程（阶段6）----

bool MainWindow::saveToPath(const QString& path)
{
    if (path.isEmpty())
        return false;
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        QMessageBox::critical(this, QStringLiteral("保存失败"),
                              QStringLiteral("无法创建项目文件夹：%1")
                                  .arg(QFileInfo(path).absolutePath()));
        return false;
    }

    const QString currentPath = m_runtimeData->currentFilePath();
    ResourceFiles::ResourceTreeUpdate resourceUpdate;
    QString error;
    if (!currentPath.isEmpty() && sameArchivePath(currentPath, path)) {
        if (!ResourceFiles::migrateLegacyResourceTree(path, error)) {
            QMessageBox::critical(this, QStringLiteral("保存失败"),
                                  QStringLiteral("迁移项目资源目录失败：\n%1").arg(error));
            return false;
        }
    } else if (!ResourceFiles::copyResourceTree(currentPath, path, resourceUpdate, error)) {
        QMessageBox::critical(
            this, QStringLiteral("保存失败"),
            QStringLiteral("复制项目资源目录失败，未保存到 %1：\n%2").arg(path, error));
        return false;
    }

    const SaveResult result = m_runtimeData->saveFile(path, m_runtimeData->fileData());
    if (!result.ok) {
        QString rollbackError;
        if (!ResourceFiles::rollbackResourceTree(resourceUpdate, rollbackError)) {
            rollbackError = QStringLiteral("\n目标资源目录恢复失败：%1").arg(rollbackError);
        }
        QMessageBox::critical(this, QStringLiteral("保存失败"),
                              QStringLiteral("无法保存到 %1：\n%2%3")
                                  .arg(path, result.message, rollbackError));
        return false;
    }
    ResourceFiles::discardResourceTreeBackup(resourceUpdate);
    return true;
}

bool MainWindow::openFromPath(const QString& path)
{
    const LoadResult result = m_runtimeData->openFile(path);
    if (!result.ok()) {
        QMessageBox::critical(this, QStringLiteral("打开失败"),
                              QStringLiteral("文件损坏或无法打开：%1\n\n%2")
                                  .arg(path, result.message));
        return false; // 失败保留现数据
    }
    return true;
}

MainWindow::UnsavedChoice MainWindow::askUnsaved()
{
    if (!m_runtimeData->isDirty())
        return UnsavedChoice::Discard;
    QMessageBox box(QMessageBox::Warning, QStringLiteral("未保存的修改"),
                    QStringLiteral("当前文件有未保存的修改，是否保存？"),
                    QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, this);
    box.button(QMessageBox::Save)->setText(QStringLiteral("保存"));
    box.button(QMessageBox::Discard)->setText(QStringLiteral("不保存"));
    box.button(QMessageBox::Cancel)->setText(QStringLiteral("取消"));
    box.setDefaultButton(QMessageBox::Save);
    switch (box.exec()) {
    case QMessageBox::Save:    return UnsavedChoice::Save;
    case QMessageBox::Discard: return UnsavedChoice::Discard;
    default:                   return UnsavedChoice::Cancel;
    }
}

bool MainWindow::maybeCloseOrContinue()
{
    switch (askUnsaved()) {
    case UnsavedChoice::Discard:
        return true;
    case UnsavedChoice::Save:
        return saveWithDialog(); // 保存失败/取消另存为对话框 = 中止
    case UnsavedChoice::Cancel:
        break;
    }
    return false;
}

bool MainWindow::chooseProjectArchivePath(QString& path)
{
    path.clear();
    const QString currentPath = m_runtimeData->currentFilePath();
    const QString initialDir = currentPath.isEmpty()
        ? QString() : QFileInfo(currentPath).absolutePath();
    const QString directory = QFileDialog::getExistingDirectory(
        this, QStringLiteral("选择项目文件夹"), initialDir);
    if (directory.isEmpty())
        return false;

    const QDir projectDir(directory);
    const QString projectName = projectDir.dirName();
    if (projectName.isEmpty() || projectDir.isRoot()) {
        QMessageBox::warning(this, QStringLiteral("保存项目"),
                             QStringLiteral("请选择独立的项目文件夹，不能使用磁盘根目录。"));
        return false;
    }

    const QString candidate = projectDir.filePath(projectName + QStringLiteral(".json"));
    const QStringList jsonFiles = projectDir.entryList({ QStringLiteral("*.json") }, QDir::Files);
    for (const QString& fileName : jsonFiles) {
        const QString existing = projectDir.filePath(fileName);
        if (!sameArchivePath(existing, candidate)) {
            QMessageBox::warning(
                this, QStringLiteral("保存项目"),
                QStringLiteral("目标文件夹已包含其他 JSON 文件，请选择独立的项目文件夹。"));
            return false;
        }
    }

    if (!currentPath.isEmpty() && !sameArchivePath(candidate, currentPath)
        && sameArchivePath(ResourceFiles::resourceRootDir(candidate),
                           ResourceFiles::resourceRootDir(currentPath))) {
        QMessageBox::warning(this, QStringLiteral("保存项目"),
                             QStringLiteral("当前项目已经位于该文件夹，不能共用 Assets 另存新项目。"));
        return false;
    }

    const bool targetExists = QFileInfo::exists(candidate)
        || QFileInfo::exists(ResourceFiles::resourceRootDir(candidate))
        || QFileInfo::exists(ResourceFiles::legacyResourceRootDir(candidate));
    if (targetExists && !sameArchivePath(candidate, currentPath)) {
        const auto choice = QMessageBox::question(
            this, QStringLiteral("覆盖项目"),
            QStringLiteral("目标文件夹已包含项目存档或资源目录，是否覆盖？\n%1").arg(candidate),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (choice != QMessageBox::Yes)
            return false;
    }

    path = candidate;
    return true;
}

bool MainWindow::saveWithDialog()
{
    if (!m_runtimeData->currentFilePath().isEmpty())
        return saveToPath(m_runtimeData->currentFilePath());
    QString path;
    if (!chooseProjectArchivePath(path))
        return false;
    return saveToPath(path);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (maybeCloseOrContinue()) {
        // 确认关闭才持久化布局（阶段9）；QSettings 写失败静默跳过，不影响关闭
        saveWindowLayout();
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::saveWindowLayout()
{
    AppSettings settings;
    settings.setWindowGeometry(saveGeometry());
    settings.setWindowState(saveState());
}

// ---- 启动页（阶段8 启动流程；"关闭项目"复用同一页）----

bool MainWindow::runStartDialogLoop()
{
    // 打开失败（openFromPath 已弹提示）→ 移除失效条目并重新弹启动页
    for (;;) {
        StartDialog dialog(this);
        if (dialog.exec() != QDialog::Accepted)
            return false; // Esc/标题栏关闭 = 不继续（调用方退出程序）
        if (dialog.choice() != StartDialog::Choice::OpenPath) {
            // 新建/空项目：重置为默认空舞台（启动时构造本就是空舞台，等效）
            m_runtimeData->newFile();
            return true;
        }
        const QString path = dialog.selectedPath();
        if (openFromPath(path))
            return true;
        AppSettings settings;
        settings.removeRecentFile(path);
    }
}

void MainWindow::closeProject()
{
    if (!maybeCloseOrContinue())
        return; // 门卫取消：留在原项目
    // 回启动页（与启动时同一页：取消它 = 退出程序）。该退出路径不走 closeEvent，
    // 退出前补一次布局落盘，与正常退出行为一致
    hide();
    if (!runStartDialogLoop()) {
        saveWindowLayout();
        QCoreApplication::quit();
        return;
    }
    show();
}

void MainWindow::updateWindowTitle()
{
    const QString path = m_runtimeData->currentFilePath();
    const QString name = path.isEmpty() ? QStringLiteral("未命名")
                                        : QFileInfo(path).fileName();
    setWindowTitle(QStringLiteral("%1[*] - %2").arg(name, QApplication::applicationDisplayName()));
    setWindowModified(m_runtimeData->isDirty());
}

void MainWindow::setupDockPanels()
{
    setDockNestingEnabled(true);
    setCorner(Qt::BottomLeftCorner, Qt::BottomDockWidgetArea);

    auto* hierarchyDock = new QDockWidget(QStringLiteral("Hierarchy"), this);
    hierarchyDock->setObjectName(QStringLiteral("hierarchyDock")); // 保留 objectName 供"窗口"菜单开关
    m_hierarchyPanel = new HierarchyPanel(m_editorCore, hierarchyDock);
    hierarchyDock->setWidget(m_hierarchyPanel);
    addDockWidget(Qt::LeftDockWidgetArea, hierarchyDock);
    m_docks.append(hierarchyDock);

    auto* inspectorDock = new QDockWidget(QStringLiteral("Inspector"), this);
    inspectorDock->setObjectName(QStringLiteral("inspectorDock"));
    m_inspectorPanel = new InspectorPanel(m_editorCore, inspectorDock);
    inspectorDock->setWidget(m_inspectorPanel);
    addDockWidget(Qt::RightDockWidgetArea, inspectorDock);
    m_docks.append(inspectorDock);

    auto* resourceDock = new QDockWidget(QStringLiteral("资源库"), this);
    resourceDock->setObjectName(QStringLiteral("resourceDock"));
    m_resourceWindow = new ResourceWindow(m_editorCore, [this] {
        if (!m_runtimeData->currentFilePath().isEmpty())
            return true;
        return saveWithDialog();
    }, resourceDock);
    resourceDock->setWidget(m_resourceWindow);
    addDockWidget(Qt::BottomDockWidgetArea, resourceDock);
    m_docks.append(resourceDock);

    auto* timelineDock = new QDockWidget(QStringLiteral("时间轴"), this);
    timelineDock->setObjectName(QStringLiteral("timelineDock"));
    m_timelineWidget = new TimelineWidget(m_editorCore, timelineDock);
    timelineDock->setWidget(m_timelineWidget);
    splitDockWidget(resourceDock, timelineDock, Qt::Horizontal);
    m_docks.append(timelineDock);
    applyDefaultResourceLayout();
}

void MainWindow::applyDefaultResourceLayout()
{
    QDockWidget* hierarchyDock = nullptr;
    QDockWidget* resourceDock = nullptr;
    QDockWidget* timelineDock = nullptr;
    for (QDockWidget* dock : m_docks) {
        if (dock->objectName() == QStringLiteral("hierarchyDock"))
            hierarchyDock = dock;
        else if (dock->objectName() == QStringLiteral("resourceDock"))
            resourceDock = dock;
        else if (dock->objectName() == QStringLiteral("timelineDock"))
            timelineDock = dock;
    }
    if (!hierarchyDock || !resourceDock || !timelineDock)
        return;

    setDockNestingEnabled(true);
    setCorner(Qt::BottomLeftCorner, Qt::BottomDockWidgetArea);
    resourceDock->show();
    splitDockWidget(resourceDock, timelineDock, Qt::Horizontal);
    const int leftWidth = qMax(220, hierarchyDock->sizeHint().width());
    resizeDocks({ hierarchyDock, resourceDock }, { leftWidth, leftWidth }, Qt::Horizontal);
    resizeDocks({ resourceDock, timelineDock }, { 260, 260 }, Qt::Vertical);
}

void MainWindow::setupMenus()
{
    QMenu* fileMenu = menuBar()->addMenu(QStringLiteral("文件(&F)"));
    // 新建/打开/保存/另存为（阶段6）：菜单动作只是取路径的薄壳，
    // 核心流程在 saveToPath/openFromPath/askUnsaved/maybeCloseOrContinue（可自测直调）
    QAction* newAction = fileMenu->addAction(QStringLiteral("新建(&N)"));
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, [this] {
        if (maybeCloseOrContinue())
            m_runtimeData->newFile();
    });
    QAction* openAction = fileMenu->addAction(QStringLiteral("打开(&O)..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, [this] {
        if (!maybeCloseOrContinue())
            return;
        const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("打开文件"),
                                                          QString(),
                                                          QString::fromLatin1(kArchiveFilter));
        if (!path.isEmpty())
            openFromPath(path);
    });
    QAction* saveAction = fileMenu->addAction(QStringLiteral("保存(&S)"));
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, [this] { saveWithDialog(); });
    QAction* saveAsAction = fileMenu->addAction(QStringLiteral("另存为(&A)..."));
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, this, [this] {
        QString path;
        if (chooseProjectArchivePath(path))
            saveToPath(path);
    });
    // 关闭项目：过未保存门卫后回启动页（与启动时同一页，可另选项目或新建）
    QAction* closeProjectAction = fileMenu->addAction(QStringLiteral("关闭项目(&C)"));
    connect(closeProjectAction, &QAction::triggered, this, &MainWindow::closeProject);
    fileMenu->addSeparator();
    QAction* quitAction = fileMenu->addAction(QStringLiteral("退出(&X)"));
    connect(quitAction, &QAction::triggered, this, &QWidget::close); // closeEvent 过未保存门卫

    QMenu* editMenu = menuBar()->addMenu(QStringLiteral("编辑(&E)"));
    // 撤销/重做（阶段4）：可用性随撤销栈状态刷新
    m_undoAction = editMenu->addAction(QStringLiteral("撤销(&U)"));
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_redoAction = editMenu->addAction(QStringLiteral("重做(&R)"));
    m_redoAction->setShortcut(QKeySequence::Redo);
    connect(m_undoAction, &QAction::triggered, m_editorCore, &EditorCore::undo);
    connect(m_redoAction, &QAction::triggered, m_editorCore, &EditorCore::redo);
    connect(m_editorCore, &EditorCore::undoStackChanged,
            this, &MainWindow::updateEditActions);
    editMenu->addSeparator();
    m_copyAction = editMenu->addAction(QStringLiteral("复制(&C)"));
    m_copyAction->setShortcut(QKeySequence::Copy);
    m_pasteAction = editMenu->addAction(QStringLiteral("粘贴(&P)"));
    m_pasteAction->setShortcut(QKeySequence::Paste);
    m_deleteAction = editMenu->addAction(QStringLiteral("删除(&D)"));
    m_deleteAction->setShortcut(QKeySequence::Delete);
    connect(m_copyAction, &QAction::triggered, m_editorCore, &EditorCore::copySelection);
    connect(m_pasteAction, &QAction::triggered,
            m_editorCore, [this] { m_editorCore->pasteClipboard(); }); // 默认偏移 (x+20, y+20)
    connect(m_deleteAction, &QAction::triggered, m_editorCore, [this] {
        // 阶段12：选中路径点时 Delete 作用于路径点（对象语义让步）；否则对象删除
        if (const auto& path = m_editorCore->activePath(); path && path->pointIndex >= 0) {
            m_editorCore->removePathPoint(path->segment.objectId, path->segment.from,
                                          path->segment.to, path->pointIndex);
            return;
        }
        m_editorCore->deleteSelection();
    });
    connect(m_editorCore->selection(), &SelectionModel::selectionChanged,
            this, &MainWindow::updateEditActions);
    connect(m_editorCore, &EditorCore::clipboardChanged,
            this, &MainWindow::updateEditActions);
    connect(m_editorCore, &EditorCore::activePathChanged,
            this, &MainWindow::updateEditActions); // 阶段12：路径点选中也使能删除
    updateEditActions();

    // 设置（阶段7 合并入口）：舞台设置与偏好设置已并入同一个统一设置窗口
    // （VSCode 风格，非模态单例），菜单栏顶层"设置"项单击即打开窗口并定位
    // "舞台"分类——menuBar 直接 addAction 的顶层项是按钮式触发，不需要下拉菜单。
    // 舞台宽/深/中心点偏移是文件数据——editingFinished 时经 EditorCore::setStage
    // 直改直提交（不入撤销栈：撤销栈只覆盖主视口/Hierarchy/Inspector/时间轴的
    // 可操作对象）；生效路径沿用 commitFileData → fileDataChanged → StageScene
    // 重建。舞台颜色/网格吸附/显示网格/打帧询问是软件自身设置（AppSettings
    // 持久化），不进文件、不弄脏 dirty、不入撤销栈，改动立即生效
    QAction* settingsAction = menuBar()->addAction(QStringLiteral("设置(&T)"));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::openSettings);

    QMenu* windowMenu = menuBar()->addMenu(QStringLiteral("窗口(&W)"));
    for (QDockWidget* dock : m_docks)
        windowMenu->addAction(dock->toggleViewAction());
}

void MainWindow::updateEditActions()
{
    m_undoAction->setEnabled(m_editorCore->canUndo());
    m_redoAction->setEnabled(m_editorCore->canRedo());
    const bool hasSelection = !m_editorCore->selection()->isEmpty();
    // 阶段12：选中路径点时删除/可用（作用于路径点）
    const auto& activePath = m_editorCore->activePath();
    const bool hasPathPoint = activePath && activePath->pointIndex >= 0;
    m_copyAction->setEnabled(hasSelection);
    m_deleteAction->setEnabled(hasSelection || hasPathPoint);
    m_pasteAction->setEnabled(m_editorCore->clipboardHasContent());
}

// ---- 统一设置窗口（阶段7）----

SettingsWindow* MainWindow::ensureSettingsWindow()
{
    if (!m_settingsWindow) {
        // 懒创建：正常启动（含 dumpScreenshotForSelfTest 主路径）不创建，
        // 只在首次菜单触发时构造；父对象为本窗口，随主窗口销毁
        m_settingsWindow = new SettingsWindow(m_runtimeData, m_editorCore, this);
        // 舞台颜色/显示网格/网格间距是软件自身设置：改动立即生效，场景即时重绘
        connect(m_settingsWindow, &SettingsWindow::stageColorChanged,
                m_scene, [this] { m_scene->update(); });
        connect(m_settingsWindow, &SettingsWindow::gridVisibilityChanged,
                m_scene, [this] { m_scene->update(); });
        connect(m_settingsWindow, &SettingsWindow::gridAppearanceChanged,
                m_scene, [this] { m_scene->update(); });
    }
    return m_settingsWindow;
}

void MainWindow::openSettings()
{
    ensureSettingsWindow()->openCategory(SettingsWindow::Category::Stage);
}
