#pragma once

// 项目资源命令（阶段14）：只修改 FileData::resources 内存副本，不进行任何磁盘
// IO（文件复制在命令构造前由 ResourceWindow 完成；删除/撤销不删除物理文件）。
// 唯一性/非空校验在 EditorCore 入口完成，命令本身不校验。

#include "Commands.h"

#include <QList>
#include <QString>

// 批量添加：一次混合导入的成功项合并为一条命令（一步撤销）。
class AddResourcesCommand : public Command {
public:
    explicit AddResourcesCommand(QList<ResourceData> resources);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    QList<ResourceData> m_resources;
};

// 移除单个资源：apply 首次执行时记录原下标与完整数据，revert 原位恢复。
class RemoveResourceCommand : public Command {
public:
    explicit RemoveResourceCommand(QString id);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    QString m_id;
    int m_index = -1;
    ResourceData m_removed;
    bool m_captured = false;
};

// 修改项目内别名（严格互逆）。
class RenameResourceCommand : public Command {
public:
    RenameResourceCommand(QString id, QString newName);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    QString m_id;
    QString m_newName;
    QString m_oldName;
    bool m_captured = false;
};
