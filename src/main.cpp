#include <QApplication>
#include <QStringList>

#include "data/AppSettings.h"
#include "render/MainWindow.h"
#include "render/StartDialog.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("StageRehearsal"));
    QCoreApplication::setApplicationName(QStringLiteral("StageRehearsal"));
    QApplication::setApplicationDisplayName(QStringLiteral("舞台剧排练预演"));

    // 验收辅助（正常启动不触发）：--selftest-screenshot=<路径.png> 截两张视口图后退出。
    // 截图模式保持旧行为：跳过启动页，直接打开示例存档
    const QString prefix = QStringLiteral("--selftest-screenshot=");
    QString screenshotPath;
    for (const QString& arg : QCoreApplication::arguments()) {
        if (arg.startsWith(prefix)) {
            screenshotPath = arg.mid(prefix.size());
            break;
        }
    }

    MainWindow window;
    window.resize(1440, 900);

    if (!screenshotPath.isEmpty()) {
        window.openFromPath(QCoreApplication::applicationDirPath()
                            + QStringLiteral("/stage-demo.json"));
        window.show();
        window.dumpScreenshotForSelfTest(screenshotPath);
        return app.exec();
    }

    // 正常启动：启动页选择要打开的项目（启动页只出现一次；关闭它 = 不启动主界面）。
    // 打开失败（openFromPath 已弹提示）→ 移除失效条目并重新弹启动页
    for (;;) {
        StartDialog dialog(&window);
        if (dialog.exec() != QDialog::Accepted)
            return 0; // Esc/标题栏关闭 = 退出程序
        if (dialog.choice() != StartDialog::Choice::OpenPath)
            break;    // 新建/空项目：MainWindow 构造已是默认空舞台
        const QString path = dialog.selectedPath();
        if (window.openFromPath(path))
            break;
        AppSettings settings;
        settings.removeRecentFile(path);
    }

    window.show();
    return app.exec();
}
