#include "ResourceFiles.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>

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

Qt::CaseSensitivity pathSensitivity()
{
#if defined(Q_OS_WIN)
    return Qt::CaseInsensitive;
#else
    return Qt::CaseSensitive;
#endif
}

bool samePath(const QString& a, const QString& b)
{
    return absoluteCleanPath(a).compare(absoluteCleanPath(b), pathSensitivity()) == 0;
}

bool pathInside(const QString& path, const QString& root)
{
    const QString absolutePath = absoluteCleanPath(path);
    const QString absoluteRoot = absoluteCleanPath(root);
    return absolutePath != absoluteRoot
        && absolutePath.startsWith(absoluteRoot + QLatin1Char('/'), pathSensitivity());
}

bool pathsOverlap(const QString& a, const QString& b)
{
    return samePath(a, b) || pathInside(a, b) || pathInside(b, a);
}

QString uniqueSiblingPath(const QString& targetPath, const QString& tag)
{
    const QFileInfo info(targetPath);
    return QDir(info.absolutePath())
        .filePath(QStringLiteral(".%1.%2.%3")
                      .arg(info.fileName(), tag,
                           QUuid::createUuid().toString(QUuid::WithoutBraces)));
}

bool removeDirectory(const QString& path, QString& error)
{
    if (!QFileInfo::exists(path))
        return true;
    if (QDir(path).removeRecursively())
        return true;
    error = QStringLiteral("无法清理目录：%1").arg(path);
    return false;
}

bool copyDirectoryContents(const QString& sourceDir, const QString& targetDir,
                           bool overwrite, QString& error)
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

    const QFileInfoList entries = source.entryInfoList(
        QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System);
    for (const QFileInfo& entry : entries) {
        if (entry.isSymLink()) {
            error = QStringLiteral("资源目录不支持链接条目：%1").arg(entry.absoluteFilePath());
            return false;
        }
        const QString targetPath = QDir(targetDir).filePath(entry.fileName());
        if (entry.isDir()) {
            if (!copyDirectoryContents(entry.absoluteFilePath(), targetPath, overwrite, error))
                return false;
            continue;
        }
        if (QFileInfo::exists(targetPath)) {
            if (!overwrite)
                continue;
            if (!QFile::remove(targetPath)) {
                error = QStringLiteral("无法覆盖目标资源文件：%1").arg(targetPath);
                return false;
            }
        }
        if (!QFile::copy(entry.absoluteFilePath(), targetPath)) {
            error = QStringLiteral("无法复制资源文件：%1 → %2")
                        .arg(entry.absoluteFilePath(), targetPath);
            return false;
        }
    }
    return true;
}

QString candidatePath(const QString& root, const QString& relativePath)
{
    const QString absoluteRoot = absoluteCleanPath(root);
    const QString candidate = absoluteCleanPath(QDir(absoluteRoot).filePath(relativePath));
    if (candidate != absoluteRoot
        && !candidate.startsWith(absoluteRoot + QLatin1Char('/'), pathSensitivity())) {
        return QString();
    }

    const QFileInfo candidateInfo(candidate);
    if (!candidateInfo.exists())
        return candidate;
    const QString canonicalRoot = QFileInfo(absoluteRoot).canonicalFilePath();
    const QString canonicalCandidate = candidateInfo.canonicalFilePath();
    if (canonicalRoot.isEmpty() || canonicalCandidate.isEmpty())
        return QString();
    const QString cleanRoot = QDir::cleanPath(canonicalRoot);
    const QString cleanCandidate = QDir::cleanPath(canonicalCandidate);
    if (cleanCandidate != cleanRoot
        && !cleanCandidate.startsWith(cleanRoot + QLatin1Char('/'), pathSensitivity())) {
        return QString();
    }
    return candidate;
}

} // namespace

QString ResourceFiles::resourceRootDir(const QString& archivePath)
{
    if (archivePath.isEmpty())
        return QString();
    return QDir(QFileInfo(archivePath).absolutePath()).filePath(QStringLiteral("Assets"));
}

QString ResourceFiles::legacyResourceRootDir(const QString& archivePath)
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
        || path.contains(QLatin1Char(':')) || QDir::isAbsolutePath(path)) {
        return false;
    }

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

    const QString normalized = normalizedRelative(relativePath);
    const QString primary = candidatePath(resourceRootDir(archivePath), normalized);
    if (!primary.isEmpty() && QFileInfo::exists(primary))
        return primary;
    const QString legacy = candidatePath(legacyResourceRootDir(archivePath), normalized);
    if (!legacy.isEmpty() && QFileInfo::exists(legacy))
        return legacy;
    return primary;
}

bool ResourceFiles::migrateLegacyResourceTree(const QString& archivePath, QString& error)
{
    error.clear();
    if (archivePath.isEmpty())
        return true;
    const QString legacyRoot = legacyResourceRootDir(archivePath);
    if (!QFileInfo::exists(legacyRoot))
        return true;
    if (!QFileInfo(legacyRoot).isDir()) {
        error = QStringLiteral("旧资源路径不是目录：%1").arg(legacyRoot);
        return false;
    }
    return copyDirectoryContents(legacyRoot, resourceRootDir(archivePath), false, error);
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
    if (!migrateLegacyResourceTree(archivePath, error))
        return false;

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
                                     const QString& targetArchivePath,
                                     ResourceTreeUpdate& update, QString& error)
{
    update = {};
    error.clear();
    if (targetArchivePath.isEmpty()) {
        error = QStringLiteral("目标存档路径为空");
        return false;
    }

    const QString targetRoot = resourceRootDir(targetArchivePath);
    QStringList sourceRoots;
    if (!sourceArchivePath.isEmpty()) {
        sourceRoots = { resourceRootDir(sourceArchivePath),
                        legacyResourceRootDir(sourceArchivePath) };
    }
    for (const QString& sourceRoot : sourceRoots) {
        if (samePath(sourceRoot, targetRoot))
            return true;
        if (QFileInfo::exists(sourceRoot) && pathsOverlap(sourceRoot, targetRoot)) {
            error = QStringLiteral("源资源目录与目标资源目录不能互相嵌套");
            return false;
        }
    }

    QString stagingRoot;
    for (const QString& sourceRoot : sourceRoots) {
        if (!QFileInfo::exists(sourceRoot))
            continue;
        if (!QFileInfo(sourceRoot).isDir()) {
            error = QStringLiteral("资源路径不是目录：%1").arg(sourceRoot);
            return false;
        }
        if (stagingRoot.isEmpty()) {
            const QFileInfo targetInfo(targetRoot);
            if (!QDir().mkpath(targetInfo.absolutePath())) {
                error = QStringLiteral("无法创建目标项目目录：%1").arg(targetInfo.absolutePath());
                return false;
            }
            stagingRoot = uniqueSiblingPath(targetRoot, QStringLiteral("staging"));
        }
        if (!copyDirectoryContents(sourceRoot, stagingRoot, false, error)) {
            QString cleanupError;
            removeDirectory(stagingRoot, cleanupError);
            return false;
        }
    }
    if (stagingRoot.isEmpty()) {
        const QFileInfo targetInfo(targetRoot);
        if (!QDir().mkpath(targetInfo.absolutePath())) {
            error = QStringLiteral("无法创建目标项目目录：%1").arg(targetInfo.absolutePath());
            return false;
        }
        stagingRoot = uniqueSiblingPath(targetRoot, QStringLiteral("staging"));
        if (!QDir().mkpath(stagingRoot)) {
            error = QStringLiteral("无法创建临时资源目录：%1").arg(stagingRoot);
            return false;
        }
    }

    QString backupRoot;
    if (QFileInfo::exists(targetRoot)) {
        if (!QFileInfo(targetRoot).isDir()) {
            error = QStringLiteral("目标资源路径不是目录：%1").arg(targetRoot);
            QString cleanupError;
            removeDirectory(stagingRoot, cleanupError);
            return false;
        }
        backupRoot = uniqueSiblingPath(targetRoot, QStringLiteral("backup"));
        if (!QDir().rename(targetRoot, backupRoot)) {
            error = QStringLiteral("无法备份目标资源目录：%1").arg(targetRoot);
            QString cleanupError;
            removeDirectory(stagingRoot, cleanupError);
            return false;
        }
    }

    if (!QDir().rename(stagingRoot, targetRoot)) {
        error = QStringLiteral("无法安装目标资源目录：%1").arg(targetRoot);
        if (!backupRoot.isEmpty() && !QDir().rename(backupRoot, targetRoot)) {
            error += QStringLiteral("\n目标资源目录恢复失败，原资源保留在：%1").arg(backupRoot);
        }
        QString cleanupError;
        removeDirectory(stagingRoot, cleanupError);
        return false;
    }

    update.targetRoot = targetRoot;
    update.backupRoot = backupRoot;
    update.changed = true;
    return true;
}

bool ResourceFiles::copyResourceTree(const QString& sourceArchivePath,
                                     const QString& targetArchivePath, QString& error)
{
    ResourceTreeUpdate update;
    if (!copyResourceTree(sourceArchivePath, targetArchivePath, update, error))
        return false;
    discardResourceTreeBackup(update);
    return true;
}

bool ResourceFiles::rollbackResourceTree(ResourceTreeUpdate& update, QString& error)
{
    error.clear();
    if (!update.changed)
        return true;
    if (!removeDirectory(update.targetRoot, error))
        return false;
    if (!update.backupRoot.isEmpty() && !QDir().rename(update.backupRoot, update.targetRoot)) {
        error = QStringLiteral("无法恢复目标资源目录：%1").arg(update.targetRoot);
        return false;
    }
    update = {};
    return true;
}

void ResourceFiles::discardResourceTreeBackup(ResourceTreeUpdate& update)
{
    if (!update.backupRoot.isEmpty())
        QDir(update.backupRoot).removeRecursively();
    update = {};
}
