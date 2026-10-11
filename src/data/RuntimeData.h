#pragma once

// 运行时数据 = 软件自身数据（AppSettings）+ 文件读入数据（FileData）。
// 对业务层只暴露只读的文件数据访问；文件数据在本层内不可更改，
// 只能整体替换（新建/打开/保存成功后同步）。保存由业务层显式调用 saveFile 触发。
// 阶段3 授权例外：commitFileData 供业务层提交编辑命令产生的 FileData 副本，
// 只替换内存数据并发 fileDataChanged()，不改文件路径、不触发保存。
// 阶段6：dirty 跟踪——保存一份最近一次保存/加载的内容快照（m_savedData），
// commitFileData 后用内容比较得出 dirty（撤销回已存内容 = 干净），
// 状态翻转时发 dirtyChanged()。

#include "AppSettings.h"
#include "DataTypes.h"
#include "JsonStore.h"

#include <QObject>

class RuntimeData : public QObject {
    Q_OBJECT
public:
    explicit RuntimeData(QObject* parent = nullptr);

    AppSettings& settings();
    const AppSettings& settings() const;

    const FileData& fileData() const;
    QString currentFilePath() const;

    void newFile();                                        // 重置为默认值，清空路径
    LoadResult openFile(const QString& path);              // 成功则替换 fileData 并记录路径
    SaveResult saveFile(const QString& path, const FileData& data); // 成功后同步为已存内容

    // 阶段3 编辑提交入口：替换内存数据并发 fileDataChanged()。
    // 不改文件路径、不触发保存；阶段6 起按内容比较刷新 dirty 状态。
    // 阶段7 起同时承载舞台尺寸直改（EditorCore::setStage——不入撤销栈的唯一例外）。
    void commitFileData(const FileData& data);

    // 阶段6：未保存修改标记。dirty =（内存 FileData ≠ 最近一次保存/加载的内容快照）；
    // newFile/openFile/saveFile 成功后 dirty=false。
    bool isDirty() const { return m_dirty; }

signals:
    // 文件整体替换（newFile / openFile 成功）时发出，先于 fileDataChanged；
    // commitFileData / saveFile 不发。阶段4 撤销栈据此清空。
    void fileReplaced();
    void fileDataChanged();
    void dirtyChanged(); // dirty 状态翻转时才发（阶段6）

private:
    void updateDirty(); // 内容比较得出 dirty，翻转时发 dirtyChanged

    AppSettings m_settings;
    FileData m_fileData;
    FileData m_savedData; // 最近一次保存/加载的内容快照（dirty 判定基准）
    QString m_currentFilePath;
    bool m_dirty = false;
};
