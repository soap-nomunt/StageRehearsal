#pragma once

// 资源试听播放器（阶段14，业务层，不依赖 Widgets）：内部使用 QMediaPlayer +
// QAudioOutput。属于运行时状态，不修改 FileData、不入存档。
//
// 异步信号防护：每次加载都新建 QMediaPlayer，回调捕获 player 指针并核对仍是
// 当前播放器，快速切换资源时旧播放器的异步信号不会污染新页面。仅别名变化
// （同 id 同相对路径）不重新加载，试听不中断；路径变化/资源失效则停止并报错。

#include "data/DataTypes.h"

#include <QObject>
#include <QString>

class QAudioOutput;
class QMediaPlayer;
class RuntimeData;

class ResourcePreviewPlayer : public QObject {
    Q_OBJECT
public:
    enum class State {
        NoResource, // 未选中/已清空
        Loading,    // 媒体加载中（异步读取元数据）
        Ready,      // 可播放
        Missing,    // 资源文件缺失
        Error,      // 解码/后端失败
    };

    explicit ResourcePreviewPlayer(RuntimeData* runtimeData, QObject* parent = nullptr);
    ~ResourcePreviewPlayer() override;

    QString resourceId() const { return m_resourceId; }
    QString relativePath() const { return m_relativePath; }
    State state() const { return m_state; }
    QString errorString() const { return m_error; }
    qint64 position() const { return m_position; }
    qint64 duration() const { return m_duration; }
    bool isPlaying() const { return m_playing; }

    void setResource(const ResourceData& resource); // 同 id 同路径不重载
    void clear();
    void play();
    void pause();
    void stop();

signals:
    void stateChanged();
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);
    void playingChanged(bool playing);

private:
    void teardownPlayer();
    void setState(State state, const QString& error = QString());
    void setPlaying(bool playing);

    RuntimeData* m_runtimeData = nullptr; // 不拥有
    QAudioOutput* m_audioOutput = nullptr;
    QMediaPlayer* m_player = nullptr;

    QString m_resourceId;
    QString m_relativePath;
    State m_state = State::NoResource;
    QString m_error;
    qint64 m_position = 0;
    qint64 m_duration = 0;
    bool m_playing = false;
};
