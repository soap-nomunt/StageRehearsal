#pragma once

// 阶段8：VS 风格启动页（模态对话框）。布局参照 Visual Studio 2022 启动窗口：
// 左侧"打开最近使用的内容"（搜索框过滤 + 最近存档列表，单击条目即打开），
// 右侧"开始使用"操作区（打开存档 / 新建项目 / 直接进入空项目）。
// Esc 或标题栏关闭 = reject（调用方据此退出程序，不显示主窗口）。
// 持有自己的 AppSettings 成员读写最近列表（与 MainWindow/RuntimeData 同一
// QSettings 存储但互不依赖），启动流程只在 main.cpp 组装。

#include "data/AppSettings.h"

#include <QDialog>
#include <QStringList>

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QStackedWidget;

class StartDialog : public QDialog
{
    Q_OBJECT
public:
    enum class Choice { NewProject, OpenPath, Empty };

    explicit StartDialog(QWidget* parent = nullptr);

    Choice choice() const { return m_choice; }
    QString selectedPath() const { return m_selectedPath; } // 仅 Choice::OpenPath 时有效

private:
    void rebuildList(); // 按搜索词从 m_allRecent 过滤重建列表（含空态占位切换）
    void acceptWith(Choice choice, const QString& path = QString());
    QWidget* makeEntryRow(QPushButton*& buttonOut, const QString& objectName,
                          const QString& title, const QString& description);

    AppSettings m_settings; // 独立持有：读写最近列表不依赖 MainWindow/RuntimeData
    QStringList m_allRecent;
    Choice m_choice = Choice::Empty;
    QString m_selectedPath;

    QLineEdit* m_searchEdit = nullptr;       // recentSearchEdit
    QStackedWidget* m_recentStack = nullptr; // recentStack（列表页 / 占位页）
    QListWidget* m_recentList = nullptr;     // recentFilesList
    QLabel* m_emptyLabel = nullptr;          // recentEmptyLabel
    QPushButton* m_openOtherButton = nullptr;    // openOtherButton
    QPushButton* m_newProjectButton = nullptr;   // newProjectButton
    QPushButton* m_emptyProjectButton = nullptr; // emptyProjectButton
};
