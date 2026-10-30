#pragma once

#include "DataTypes.h"

#include <QString>

namespace ResourceFiles {

struct ResourceTreeUpdate {
    QString targetRoot;
    QString backupRoot;
    bool changed = false;
};

QString resourceRootDir(const QString& archivePath);
QString legacyResourceRootDir(const QString& archivePath);
QString typeSubdir(ResourceType type);
bool isSafeRelativePath(const QString& relativePath);
QString resolveResourcePath(const QString& archivePath, const QString& relativePath);
bool migrateLegacyResourceTree(const QString& archivePath, QString& error);

bool importResourceFile(const QString& archivePath, ResourceType type,
                        const QString& resourceId, const QString& sourcePath,
                        QString& relativePath, QString& error);

bool copyResourceTree(const QString& sourceArchivePath, const QString& targetArchivePath,
                      ResourceTreeUpdate& update, QString& error);
bool copyResourceTree(const QString& sourceArchivePath, const QString& targetArchivePath,
                      QString& error);
bool rollbackResourceTree(ResourceTreeUpdate& update, QString& error);
void discardResourceTreeBackup(ResourceTreeUpdate& update);

} // namespace ResourceFiles
