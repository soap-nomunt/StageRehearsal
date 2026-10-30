#include "ResourcePreviewPlayer.h"

#include "data/ResourceFiles.h"
#include "data/RuntimeData.h"

#include <QAudioOutput>
#include <QFileInfo>
#include <QMediaPlayer>
#include <QUrl>

ResourcePreviewPlayer::ResourcePreviewPlayer(RuntimeData* runtimeData, QObject* parent)
    : QObject(parent)
    , m_runtimeData(runtimeData)
{
    // QAudioOutput/QMediaPlayer 延迟到首次加载音频时创建：不试听时（含数据层
    // 自测等无 GUI 场景）不触碰多媒体后端。
}

ResourcePreviewPlayer::~ResourcePreviewPlayer()
{
    disconnect(); // 析构期间不再向已连接的界面发信号
    teardownPlayer();
}

void ResourcePreviewPlayer::teardownPlayer()
{
    if (m_player) {
        m_player->disconnect(this);
        m_player->stop();
        delete m_player;
        m_player = nullptr;
    }
    if (m_playing) {
        m_playing = false;
        emit playingChanged(false);
    }
}

void ResourcePreviewPlayer::setState(State state, const QString& error)
{
    bool stateChangedNow = m_state != state;
    m_state = state;
    if (m_error != error) {
        m_error = error;
        stateChangedNow = true;
    }
    if (stateChangedNow)
        emit stateChanged();
}

void ResourcePreviewPlayer::setPlaying(bool playing)
{
    if (m_playing == playing)
        return;
    m_playing = playing;
    emit playingChanged(playing);
}

void ResourcePreviewPlayer::setResource(const ResourceData& resource)
{
    if (m_resourceId == resource.id && m_relativePath == resource.relativePath)
        return; // 别名等非路径变化：保持当前试听

    teardownPlayer();
    m_resourceId = resource.id;
    m_relativePath = resource.relativePath;
    m_position = 0;
    m_duration = 0;
    setState(State::Loading);
    emit positionChanged(m_position);
    emit durationChanged(m_duration);

    const QString absolutePath =
        ResourceFiles::resolveResourcePath(m_runtimeData->currentFilePath(), resource.relativePath);
    if (absolutePath.isEmpty() || !QFileInfo::exists(absolutePath)) {
        setState(State::Missing, QStringLiteral("资源文件缺失：%1").arg(resource.relativePath));
        return;
    }

    if (!m_audioOutput) {
        m_audioOutput = new QAudioOutput(this);
        m_audioOutput->setVolume(1.0);
    }
    auto* player = new QMediaPlayer(this);
    player->setAudioOutput(m_audioOutput);
    m_player = player;

    connect(player, &QMediaPlayer::mediaStatusChanged, this,
            [this, player, id = resource.id](QMediaPlayer::MediaStatus status) {
                if (player != m_player || id != m_resourceId)
                    return; // 旧播放器的迟到信号
                switch (status) {
                case QMediaPlayer::LoadingMedia:
                    setState(State::Loading);
                    break;
                case QMediaPlayer::LoadedMedia:
                case QMediaPlayer::BufferedMedia:
                    setState(State::Ready);
                    break;
                case QMediaPlayer::InvalidMedia:
                    setState(State::Error, player->errorString().isEmpty()
                                               ? QStringLiteral("无法播放该音频文件")
                                               : player->errorString());
                    break;
                default:
                    break;
                }
            });
    connect(player, &QMediaPlayer::durationChanged, this,
            [this, player, id = resource.id](qint64 duration) {
                if (player != m_player || id != m_resourceId)
                    return;
                m_duration = duration;
                emit durationChanged(duration);
            });
    connect(player, &QMediaPlayer::positionChanged, this,
            [this, player, id = resource.id](qint64 position) {
                if (player != m_player || id != m_resourceId)
                    return;
                m_position = position;
                emit positionChanged(position);
            });
    connect(player, &QMediaPlayer::playbackStateChanged, this,
            [this, player, id = resource.id](QMediaPlayer::PlaybackState state) {
                if (player != m_player || id != m_resourceId)
                    return;
                setPlaying(state == QMediaPlayer::PlayingState);
            });
    connect(player, &QMediaPlayer::errorOccurred, this,
            [this, player, id = resource.id](QMediaPlayer::Error, const QString& errorString) {
                if (player != m_player || id != m_resourceId)
                    return;
                setState(State::Error, errorString.isEmpty()
                                           ? QStringLiteral("音频加载失败")
                                           : errorString);
            });

    player->setSource(QUrl::fromLocalFile(absolutePath));
}

void ResourcePreviewPlayer::clear()
{
    const bool hadResource = !m_resourceId.isEmpty() || m_player != nullptr;
    teardownPlayer();
    m_resourceId.clear();
    m_relativePath.clear();
    m_position = 0;
    m_duration = 0;
    if (hadResource || m_state != State::NoResource) {
        setState(State::NoResource);
        emit positionChanged(m_position);
        emit durationChanged(m_duration);
    }
}

void ResourcePreviewPlayer::play()
{
    if (!m_player || m_state == State::Missing || m_state == State::Error)
        return;
    m_player->play();
}

void ResourcePreviewPlayer::pause()
{
    if (m_player)
        m_player->pause();
}

void ResourcePreviewPlayer::stop()
{
    if (!m_player)
        return;
    m_player->stop();
}
