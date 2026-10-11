#include "RuntimeData.h"

RuntimeData::RuntimeData(QObject* parent)
    : QObject(parent)
    , m_fileData(FileData::createDefault())
    , m_savedData(FileData::createDefault())
{
}

AppSettings& RuntimeData::settings()
{
    return m_settings;
}

const AppSettings& RuntimeData::settings() const
{
    return m_settings;
}

const FileData& RuntimeData::fileData() const
{
    return m_fileData;
}

QString RuntimeData::currentFilePath() const
{
    return m_currentFilePath;
}

void RuntimeData::newFile()
{
    m_fileData = FileData::createDefault();
    m_savedData = m_fileData;
    m_currentFilePath.clear();
    emit fileReplaced();
    emit fileDataChanged();
    updateDirty();
}

LoadResult RuntimeData::openFile(const QString& path)
{
    LoadResult result = JsonStore::load(path);
    if (result.ok()) {
        m_fileData = result.data;
        m_savedData = m_fileData;
        m_currentFilePath = path;
        m_settings.addRecentFile(path); // 阶段8：成功才入 MRU，失败不记录
        emit fileReplaced();
        emit fileDataChanged();
        updateDirty();
    }
    return result;
}

SaveResult RuntimeData::saveFile(const QString& path, const FileData& data)
{
    SaveResult result = JsonStore::save(path, data);
    if (result.ok) {
        m_fileData = data;
        m_savedData = data;
        m_currentFilePath = path;
        m_settings.addRecentFile(path); // 阶段8：另存为后的当前文件也入 MRU
        emit fileDataChanged();
        updateDirty();
    }
    return result;
}

void RuntimeData::commitFileData(const FileData& data)
{
    m_fileData = data;
    emit fileDataChanged();
    updateDirty();
}

void RuntimeData::updateDirty()
{
    const bool dirty = m_fileData != m_savedData;
    if (dirty != m_dirty) {
        m_dirty = dirty;
        emit dirtyChanged();
    }
}
