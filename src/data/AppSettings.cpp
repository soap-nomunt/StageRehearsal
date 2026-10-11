#include "AppSettings.h"

#include <QDir>

namespace {
const char kKeyGridSnap[] = "gridSnapEnabled";
const char kKeyGridVisible[] = "gridVisible";
const char kKeyKeyframePromptSuppressed[] = "keyframePromptSuppressed";
const char kKeyGridSize[] = "gridSizeCm";
const char kKeySnapRange[] = "snapRangeCm";
const char kKeyWindowGeometry[] = "windowGeometry";
const char kKeyWindowState[] = "windowState";
const char kKeyStageColor[] = "stageColor";
const char kKeyRecentFiles[] = "recentFiles";

// 最近列表的路径比较：分隔符归一 + 大小写不敏感（Windows 文件系统语义）
bool sameRecentPath(const QString& a, const QString& b)
{
    return QDir::fromNativeSeparators(a).compare(QDir::fromNativeSeparators(b),
                                                 Qt::CaseInsensitive)
        == 0;
}
} // namespace

AppSettings::AppSettings() = default;

QList<QColor> AppSettings::defaultPalette()
{
    return {
        QColor(QStringLiteral("#e74c3c")),
        QColor(QStringLiteral("#3498db")),
        QColor(QStringLiteral("#2ecc71")),
        QColor(QStringLiteral("#f39c12")),
        QColor(QStringLiteral("#9b59b6")),
        QColor(QStringLiteral("#1abc9c")),
        QColor(QStringLiteral("#e67e22")),
        QColor(QStringLiteral("#95a5a6")),
    };
}

bool AppSettings::gridSnapEnabled() const
{
    return m_settings.value(QLatin1String(kKeyGridSnap), defaultGridSnapEnabled).toBool();
}

void AppSettings::setGridSnapEnabled(bool enabled)
{
    m_settings.setValue(QLatin1String(kKeyGridSnap), enabled);
}

bool AppSettings::gridVisible() const
{
    return m_settings.value(QLatin1String(kKeyGridVisible), defaultGridVisible).toBool();
}

void AppSettings::setGridVisible(bool visible)
{
    m_settings.setValue(QLatin1String(kKeyGridVisible), visible);
}

bool AppSettings::keyframePromptSuppressed() const
{
    return m_settings.value(QLatin1String(kKeyKeyframePromptSuppressed),
                            defaultKeyframePromptSuppressed)
        .toBool();
}

void AppSettings::setKeyframePromptSuppressed(bool suppressed)
{
    m_settings.setValue(QLatin1String(kKeyKeyframePromptSuppressed), suppressed);
}

double AppSettings::gridSizeCm() const
{
    return m_settings.value(QLatin1String(kKeyGridSize), defaultGridSizeCm).toDouble();
}

void AppSettings::setGridSizeCm(double sizeCm)
{
    if (sizeCm <= 0.0)
        return; // 防御：网格间距必须为正（吸附按它取模，除零保护）；正常取值由设置窗口钳制
    m_settings.setValue(QLatin1String(kKeyGridSize), sizeCm);
}

double AppSettings::snapRangeCm() const
{
    return m_settings.value(QLatin1String(kKeySnapRange), defaultSnapRangeCm).toDouble();
}

void AppSettings::setSnapRangeCm(double rangeCm)
{
    if (rangeCm <= 0.0)
        return; // 防御：吸附距离必须为正；正常取值由设置窗口钳制
    m_settings.setValue(QLatin1String(kKeySnapRange), rangeCm);
}

QByteArray AppSettings::windowGeometry() const
{
    return m_settings.value(QLatin1String(kKeyWindowGeometry)).toByteArray();
}

void AppSettings::setWindowGeometry(const QByteArray& geometry)
{
    m_settings.setValue(QLatin1String(kKeyWindowGeometry), geometry);
}

QByteArray AppSettings::windowState() const
{
    return m_settings.value(QLatin1String(kKeyWindowState)).toByteArray();
}

void AppSettings::setWindowState(const QByteArray& state)
{
    m_settings.setValue(QLatin1String(kKeyWindowState), state);
}

QColor AppSettings::defaultStageColor()
{
    return QColor(QStringLiteral("#a0a0a0"));
}

QColor AppSettings::stageColor() const
{
    // 用户未改色时返回默认值常量；改色后才有 stageColor 设置项
    return m_settings.value(QLatin1String(kKeyStageColor), defaultStageColor()).value<QColor>();
}

void AppSettings::setStageColor(const QColor& color)
{
    m_settings.setValue(QLatin1String(kKeyStageColor), color);
}

QStringList AppSettings::recentFiles() const
{
    return m_settings.value(QLatin1String(kKeyRecentFiles)).toStringList();
}

void AppSettings::addRecentFile(const QString& path)
{
    if (path.isEmpty())
        return;
    QStringList list = recentFiles();
    for (int i = list.size() - 1; i >= 0; --i) {
        if (sameRecentPath(list.at(i), path))
            list.removeAt(i);
    }
    list.prepend(path); // 去重后置顶，保留本次写入的写法
    while (list.size() > kMaxRecentFiles)
        list.removeLast();
    m_settings.setValue(QLatin1String(kKeyRecentFiles), list);
}

void AppSettings::removeRecentFile(const QString& path)
{
    QStringList list = recentFiles();
    for (int i = list.size() - 1; i >= 0; --i) {
        if (sameRecentPath(list.at(i), path))
            list.removeAt(i);
    }
    m_settings.setValue(QLatin1String(kKeyRecentFiles), list);
}
