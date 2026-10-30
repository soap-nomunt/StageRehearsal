#include "ResourceFiles.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace {

QString normalizedRelative(const QString& relativePath)
{
    QString normalized = relativePath;
    normalized.replace(QLatin1Char('\\'), QLatin1Char('/'));
    return normalized;
}

QString absoluteCleanPath(const QString& path)
{
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}

bool samePath(const QString& a, const QString& b)
{
    const QString left = absoluteCleanPath(a);
    const QString right = absoluteCleanPath(b);
#if defined(Q_OS_WIN)
    return left.compare(right, Qt::CaseInsensitive) == 0;
#else
    return left == right;
#endif
}

bool copyDirectoryContents(const QString& sourceDir, const QString& targetDir, QString& error)
{
    QDir source(sourceDir);
    if (!source.exists()) {
        error = QStringLiteral("资源目录不存在：%1").arg(sourceDir);
        return false;
    }
    if (!QDir().mkpath(targetDir)) {
        error = QStringLiteral("无法创建目标资源目录：%1").arg(targetDir);
        return false;
    }

    const QFileInfoList entries =
        source.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden);
    for (const QFileInfo& entry : entries) {
        const QString targetPath = QDir(targetDir).filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyDirectoryContents(entry.absoluteFilePath(), targetPath, error))
                return false;
            continue;
        }
        if (QFileInfo::exists(targetPath) && !QFile::remove(targetPath)) {
            error = QStringLiteral("无法覆盖目标资源文件：%1").arg(targetPath);
            return false;
        }
        if (!QFile::copy(entry.absoluteFilePath(), targetPath)) {
            error = QStringLiteral("无法复制资源文件：%1 → %2")
                        .arg(entry.absoluteFilePath(), targetPath);
            return false;
        }
    }
    return true;
}

} // namespace

QString ResourceFiles::resourceRootDir(const QString& archivePath)
{
    if (archivePath.isEmpty())
        return QString();
    const QFileInfo info(archivePath);
    QString stem = info.completeBaseName();
    if (stem.isEmpty())
        stem = QStringLiteral("project");
    return QDir(info.absolutePath()).filePath(stem + QStringLiteral(".resources"));
}

QString ResourceFiles::typeSubdir(ResourceType type)
{
    return type == ResourceType::Image ? QStringLiteral("images") : QStringLiteral("audio");
}

bool ResourceFiles::isSafeRelativePath(const QString& relativePath)
{
    if (relativePath.isEmpty())
        return false;
    const QString path = normalizedRelative(relativePath);
    if (path.isEmpty() || path.startsWith(QLatin1Char('/'))
        || path.contains(QLatin1Char(':')) || QDir::isAbsolutePath(path))
        return false;

    const QString cleaned = QDir::cleanPath(path);
    if (cleaned != path || cleaned.isEmpty() || cleaned == QLatin1String("."))
        return false;
    const QStringList segments = cleaned.split(QLatin1Char('/'));
    for (const QString& segment : segments) {
        if (segment.isEmpty() || segment == QLatin1String("..") || segment == QLatin1String("."))
            return false;
    }
    return true;
}

QString ResourceFiles::resolveResourcePath(const QString& archivePath, const QString& relativePath)
{
    if (archivePath.isEmpty() || !isSafeRelativePath(relativePath))
        return QString();
    const QString root = absoluteCleanPath(resourceRootDir(archivePath));
    const QString resolved = absoluteCleanPath(QDir(root).filePath(normalizedRelative(relativePath)));
#if defined(Q_OS_WIN)
    const Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity sensitivity = Qt::CaseSensitive;
#endif
    if (resolved != root
        && !resolved.startsWith(root + QLatin1Char('/'), sensitivity))
        return QString();
    return resolved;
}

bool ResourceFiles::importResourceFile(const QString& archivePath, ResourceType type,
                                       const QString& resourceId, const QString& sourcePath,
                                       QString& relativePath, QString& error)
{
    relativePath.clear();
    error.clear();

    if (archivePath.isEmpty()) {
        error = QStringLiteral("当前项目尚未保存，无法确定资源目录");
        return false;
    }
    if (resourceId.isEmpty() || resourceId.contains(QLatin1Char('/'))
        || resourceId.contains(QLatin1Char('\\')) || resourceId.contains(QLatin1Char(':'))) {
        error = QStringLiteral("资源 ID 非法");
        return false;
    }
    const QFileInfo sourceInfo(sourcePath);
    if (!sourceInfo.exists() || !sourceInfo.isFile() || !sourceInfo.isReadable()) {
        error = QStringLiteral("文件不存在或不可读取");
        return false;
    }

    const QString root = resourceRootDir(archivePath);
    const QString subdir = typeSubdir(type);
    const QString targetDir = QDir(root).filePath(subdir);
    if (!QDir().mkpath(targetDir)) {
        error = QStringLiteral("无法创建资源目录：%1").arg(targetDir);
        return false;
    }

    const QString suffix = sourceInfo.suffix();
    const QString extension = suffix.isEmpty() ? QString() : QStringLiteral(".") + suffix;
    QString targetPath = QDir(targetDir).filePath(resourceId + extension);
    for (int n = 2; QFileInfo::exists(targetPath); ++n) {
        targetPath = QDir(targetDir).filePath(
            QStringLiteral("%1-%2%3").arg(resourceId).arg(n).arg(extension));
    }

    if (!QFile::copy(sourceInfo.absoluteFilePath(), targetPath)) {
        error = QStringLiteral("复制文件失败：%1").arg(sourceInfo.fileName());
        return false;
    }

    relativePath = subdir + QLatin1Char('/') + QFileInfo(targetPath).fileName();
    return true;
}

bool ResourceFiles::copyResourceTree(const QString& sourceArchivePath,
                                     const QString& targetArchivePath, QString& error)
{
    error.clear();
    if (targetArchivePath.isEmpty()) {
        error = QStringLiteral("目标存档路径为空");
        return false;
    }
    const QString sourceRoot = resourceRootDir(sourceArchivePath);
    const QString targetRoot = resourceRootDir(targetArchivePath);
    if (sourceRoot.isEmpty() || sourceRoot == targetRoot || samePath(sourceRoot, targetRoot))
        return true; // 新项目无资源目录 / 保存到同一位置：无需复制

    const QFileInfo sourceInfo(sourceRoot);
    if (!sourceInfo.exists())
        return true; // 尚无资源目录
    if (!sourceInfo.isDir()) {
        error = QStringLiteral("资源路径不是目录：%1").arg(sourceRoot);
        return false;
    }
#if defined(Q_OS_WIN)
    const Qt::CaseSensitivity sensitivity = Qt::CaseInsensitive;
#else
    const Qt::CaseSensitivity sensitivity = Qt::CaseSensitive;
#endif
    if (absoluteCleanPath(targetRoot).startsWith(absoluteCleanPath(sourceRoot) + QLatin1Char('/'),
                                                 sensitivity)) {
        error = QStringLiteral("目标资源目录不能位于当前资源目录内");
        return false; // 防止递归复制自身
    }
    return copyDirectoryContents(sourceRoot, targetRoot, error);
}
