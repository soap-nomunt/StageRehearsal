#include <QApplication>
#include <QStringList>

#include "render/MainWindow.h"

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

    if (!screenshotPath.isEmpty()) {
        // 截图自测固定 1440×900：真实设置区可能存有用户布局，构造内 restore 已改过
        // 尺寸，截图模式必须在打开存档前重钉尺寸，保证与既有验收图可比
        window.resize(1440, 900);
        window.openFromPath(QCoreApplication::applicationDirPath()
                            + QStringLiteral("/stage-demo.json"));
        window.show();
        window.dumpScreenshotForSelfTest(screenshotPath);
        return app.exec();
    }

    // 正常启动：启动页选择要打开的项目（"关闭项目"也会回到该页；关闭它 = 退出程序）。
    if (!window.runStartDialogLoop())
        return 0;

    window.show();
    return app.exec();
}
