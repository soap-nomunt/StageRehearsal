#include "ResourceWindow.h"

#include "business/EditorCore.h"
#include "data/ResourceFiles.h"
#include "data/RuntimeData.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QListWidget>
#include <QMediaFormat>
#include <QMessageBox>
#include <QPushButton>
#include <QStackedWidget>
#include <QStyle>
#include <QVBoxLayout>

#include <functional>

namespace {

const char kResourceFilter[] =
    "支持的资源 (*.mp3 *.wav *.flac *.ogg *.m4a *.aac *.wma "
    "*.png *.jpg *.jpeg *.bmp *.gif *.tif *.tiff *.webp);;"
    "音频 (*.mp3 *.wav *.flac *.ogg *.m4a *.aac *.wma);;"
    "图片 (*.png *.jpg *.jpeg *.bmp *.gif *.tif *.tiff *.webp);;"
    "所有文件 (*)";

bool audioFormatForSuffix(const QString& suffix, QMediaFormat::FileFormat& out)
{
    if (suffix == QLatin1String("mp3"))  { out = QMediaFormat::MP3; return true; }
    if (suffix == QLatin1String("wav"))  { out = QMediaFormat::Wave; return true; }
    if (suffix == QLatin1String("flac")) { out = QMediaFormat::FLAC; return true; }
    if (suffix == QLatin1String("ogg") || suffix == QLatin1String("oga")) {
        out = QMediaFormat::Ogg;
        return true;
    }
    if (suffix == QLatin1String("m4a"))  { out = QMediaFormat::Mpeg4Audio; return true; }
    if (suffix == QLatin1String("aac"))  { out = QMediaFormat::AAC; return true; }
    if (suffix == QLatin1String("wma"))  { out = QMediaFormat::WMA; return true; }
    return false;
}

bool isImageSuffix(const QString& suffix)
{
    static const QSet<QString> suffixes = {
        QStringLiteral("png"), QStringLiteral("jpg"), QStringLiteral("jpeg"),
        QStringLiteral("bmp"), QStringLiteral("gif"), QStringLiteral("tif"),
        QStringLiteral("tiff"), QStringLiteral("webp"),
    };
    return suffixes.contains(suffix);
}

// 列表控件：Delete 键只在资源库内移除选中资源，不串联主窗口对象删除语义
class ResourceListWidget : public QListWidget {
public:
    explicit ResourceListWidget(QWidget* parent = nullptr)
        : QListWidget(parent)
    {
    }

    std::function<void()> onDelete;

protected:
    void keyPressEvent(QKeyEvent* event) override
    {
        if (event->key() == Qt::Key_Delete && onDelete) {
            onDelete();
            event->accept();
            return;
        }
        QListWidget::keyPressEvent(event);
    }

    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::ShortcutOverride) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Delete) {
                event->accept();
                return true;
            }
        }
        return QListWidget::event(event);
    }
};

} // namespace

ResourceWindow::ResourceWindow(EditorCore* editorCore, SaveGate ensureSaved, QWidget* parent)
    : QWidget(parent)
    , m_editorCore(editorCore)
    , m_ensureSaved(std::move(ensureSaved))
{
    auto* layout = new QVBoxLayout(this);
    auto* buttons = new QHBoxLayout();
    m_importButton = new QPushButton(QStringLiteral("导入…"), this);
    m_importButton->setObjectName(QStringLiteral("resourceImportButton"));
    m_removeButton = new QPushButton(QStringLiteral("移除"), this);
    m_removeButton->setObjectName(QStringLiteral("resourceRemoveButton"));
    buttons->addWidget(m_importButton);
    buttons->addWidget(m_removeButton);
    buttons->addStretch();
    layout->addLayout(buttons);

    m_stack = new QStackedWidget(this);
    m_emptyLabel = new QLabel(QStringLiteral("资源库为空\n点击“导入…”添加音频或图片"), this);
    m_emptyLabel->setObjectName(QStringLiteral("resourceEmptyLabel"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet(QStringLiteral("color: #888888;"));
    auto* list = new ResourceListWidget(this);
    m_list = list;
    m_list->setObjectName(QStringLiteral("resourceList"));
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setIconSize(QSize(20, 20));
    m_stack->addWidget(m_emptyLabel);
    m_stack->addWidget(m_list);
    layout->addWidget(m_stack, 1);

    connect(m_importButton, &QPushButton::clicked, this, &ResourceWindow::importResources);
    connect(m_removeButton, &QPushButton::clicked, this, &ResourceWindow::removeActive);
    connect(m_list, &QListWidget::itemSelectionChanged, this, [this] {
        if (m_updating)
            return;
        const QList<QListWidgetItem*> selected = m_list->selectedItems();
        const QListWidgetItem* item = selected.isEmpty() ? nullptr : selected.first();
        m_editorCore->setActiveResource(item ? item->data(Qt::UserRole).toString() : QString());
    });
    list->onDelete = [this] { removeActive(); };

    connect(m_editorCore, &EditorCore::activeResourceChanged,
            this, &ResourceWindow::syncSelection);
    connect(m_editorCore->runtimeData(), &RuntimeData::fileDataChanged,
            this, &ResourceWindow::refreshList);

    refreshList();
}

void ResourceWindow::refreshList()
{
    const QString activeId = m_editorCore->activeResourceId();
    m_updating = true;
    m_list->clear();
    const QList<ResourceData>& resources = m_editorCore->runtimeData()->fileData().resources;
    for (const ResourceData& resource : resources) {
        const QIcon icon = style()->standardIcon(
            resource.type == ResourceType::Audio ? QStyle::SP_MediaVolume : QStyle::SP_FileIcon);
        auto* item = new QListWidgetItem(icon, resource.name, m_list);
        item->setData(Qt::UserRole, resource.id);
        item->setToolTip(resource.relativePath);
    }
    m_stack->setCurrentWidget(resources.isEmpty() ? static_cast<QWidget*>(m_emptyLabel)
                                                  : static_cast<QWidget*>(m_list));
    m_updating = false;
    syncSelection();
}

void ResourceWindow::syncSelection()
{
    if (m_updating)
        return;
    const QString activeId = m_editorCore->activeResourceId();
    m_removeButton->setEnabled(!activeId.isEmpty());
    m_updating = true;
    QListWidgetItem* target = nullptr;
    for (int i = 0; i < m_list->count(); ++i) {
        QListWidgetItem* item = m_list->item(i);
        if (item->data(Qt::UserRole).toString() == activeId) {
            target = item;
            break;
        }
    }
    if (target) {
        m_list->setCurrentItem(target);
        m_list->scrollToItem(target);
    } else {
        m_list->setCurrentItem(nullptr);
        m_list->clearSelection();
    }
    m_updating = false;
}

void ResourceWindow::importResources()
{
    if (!m_ensureSaved()) // 未保存项目：取消保存/保存失败都中止导入
        return;
    const QString archivePath = m_editorCore->runtimeData()->currentFilePath();
    if (archivePath.isEmpty())
        return;

    const QStringList files = QFileDialog::getOpenFileNames(
        this, QStringLiteral("导入资源"), QString(), QString::fromLatin1(kResourceFilter));
    if (files.isEmpty())
        return;

    const QList<QMediaFormat::FileFormat> supportedAudioFormats =
        QMediaFormat().supportedFileFormats(QMediaFormat::Decode);
    // 后端未枚举（空列表）时不据此拒绝：扩展名通过后交给播放器加载，
    // 失败在 Inspector 明确显示；有枚举时按实际解码能力过滤
    const bool hasAudioFormatReport = !supportedAudioFormats.isEmpty();

    QList<ResourceData> imported;
    QSet<QString> reservedNames;
    QStringList failures;

    for (const QString& file : files) {
        const QFileInfo info(file);
        const QString suffix = info.suffix().toLower();
        if (!info.exists() || !info.isFile() || !info.isReadable()) {
            failures.append(QStringLiteral("%1：文件不存在或不可读取").arg(info.fileName()));
            continue;
        }

        ResourceType type = ResourceType::Image;
        if (isImageSuffix(suffix)) {
            QImageReader reader(file);
            reader.setDecideFormatFromContent(true);
            if (!reader.canRead()) {
                failures.append(QStringLiteral("%1：无法识别的图片格式").arg(info.fileName()));
                continue;
            }
        } else {
            QMediaFormat::FileFormat format = QMediaFormat::UnspecifiedFormat;
            if (!audioFormatForSuffix(suffix, format)) {
                failures.append(QStringLiteral("%1：不是支持的音频或图片文件")
                                    .arg(info.fileName()));
                continue;
            }
            if (hasAudioFormatReport && !supportedAudioFormats.contains(format)) {
                failures.append(QStringLiteral("%1：当前音频后端不支持该格式")
                                    .arg(info.fileName()));
                continue;
            }
            type = ResourceType::Audio;
        }

        const QString id = m_editorCore->generateResourceId();
        const QString name = m_editorCore->uniqueResourceName(info.completeBaseName(),
                                                              reservedNames);
        QString relativePath;
        QString error;
        if (!ResourceFiles::importResourceFile(archivePath, type, id, file,
                                               relativePath, error)) {
            failures.append(QStringLiteral("%1：%2").arg(info.fileName(), error));
            continue;
        }

        ResourceData resource;
        resource.id = id;
        resource.type = type;
        resource.name = name;
        resource.originalName = info.fileName();
        resource.relativePath = relativePath;
        imported.append(resource);
        reservedNames.insert(name);
    }

    bool committed = false;
    if (!imported.isEmpty()) {
        committed = m_editorCore->addResources(imported);
        if (committed) {
            m_editorCore->setActiveResource(imported.first().id);
        } else {
            failures.append(QStringLiteral("资源清单提交失败，已复制的文件未加入资源库"));
        }
    }

    if (!failures.isEmpty()) {
        QMessageBox::warning(
            this, QStringLiteral("导入资源"),
            QStringLiteral("以下文件未能导入：\n\n%1").arg(failures.join(QLatin1Char('\n'))));
    }
}

void ResourceWindow::removeActive()
{
    const QString id = m_editorCore->activeResourceId();
    if (id.isEmpty())
        return;
    m_editorCore->removeResource(id); // 只删清单，不删除磁盘文件
}
