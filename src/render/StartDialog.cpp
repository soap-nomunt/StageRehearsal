#include "StartDialog.h"

#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace {
const char kArchiveFilter[] = "舞台剧存档 (*.json)"; // 与 MainWindow 文件对话框一致
const char kGrayTextStyle[] = "color: #888888;";    // 灰色小字，沿用设置窗口风格
} // namespace

StartDialog::StartDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("舞台剧排练预演"));
    resize(880, 520);
    setMinimumSize(680, 420);

    m_allRecent = m_settings.recentFiles();

    auto* rootLayout = new QHBoxLayout(this);
    rootLayout->setContentsMargins(24, 20, 24, 20);
    rootLayout->setSpacing(32);

    // ---- 左侧：打开最近使用的内容 ----
    auto* leftLayout = new QVBoxLayout;
    leftLayout->setSpacing(8);
    rootLayout->addLayout(leftLayout, 1);

    auto* recentTitle = new QLabel(QStringLiteral("打开最近使用的内容"), this);
    QFont recentTitleFont = recentTitle->font();
    recentTitleFont.setBold(true);
    recentTitleFont.setPointSize(recentTitleFont.pointSize() + 2);
    recentTitle->setFont(recentTitleFont);
    leftLayout->addWidget(recentTitle);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setObjectName(QStringLiteral("recentSearchEdit"));
    m_searchEdit->setPlaceholderText(QStringLiteral("搜索最近使用的存档"));
    m_searchEdit->setClearButtonEnabled(true);
    leftLayout->addWidget(m_searchEdit);

    // 列表页 / 空态占位页二选一显示
    m_recentStack = new QStackedWidget(this);
    m_recentStack->setObjectName(QStringLiteral("recentStack"));
    leftLayout->addWidget(m_recentStack, 1);

    m_recentList = new QListWidget(this);
    m_recentList->setObjectName(QStringLiteral("recentFilesList"));
    m_recentList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_recentStack->addWidget(m_recentList);

    m_emptyLabel = new QLabel(this);
    m_emptyLabel->setObjectName(QStringLiteral("recentEmptyLabel"));
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet(QString::fromLatin1(kGrayTextStyle));
    m_recentStack->addWidget(m_emptyLabel);

    // ---- 右侧：开始使用 ----
    auto* rightLayout = new QVBoxLayout;
    rightLayout->setSpacing(14);
    rootLayout->addLayout(rightLayout, 1);

    auto* startTitle = new QLabel(QStringLiteral("开始使用"), this);
    QFont startTitleFont = startTitle->font();
    startTitleFont.setBold(true);
    startTitleFont.setPointSize(startTitleFont.pointSize() + 2);
    startTitle->setFont(startTitleFont);
    rightLayout->addWidget(startTitle);

    rightLayout->addWidget(makeEntryRow(m_openOtherButton,
                                        QStringLiteral("openOtherButton"),
                                        QStringLiteral("打开存档(&O)..."),
                                        QStringLiteral("浏览本机存档文件")));
    rightLayout->addWidget(makeEntryRow(m_newProjectButton,
                                        QStringLiteral("newProjectButton"),
                                        QStringLiteral("新建项目(&N)"),
                                        QStringLiteral("从默认空舞台开始")));
    rightLayout->addWidget(makeEntryRow(m_emptyProjectButton,
                                        QStringLiteral("emptyProjectButton"),
                                        QStringLiteral("直接进入空项目(&E)"),
                                        QStringLiteral("不打开任何文件")));
    rightLayout->addStretch();

    // 原生文件对话框 offscreen 不可驱动，列手测；取消则留在启动页
    connect(m_openOtherButton, &QPushButton::clicked, this, [this] {
        const QString path =
            QFileDialog::getOpenFileName(this, QStringLiteral("打开存档"), QString(),
                                         QString::fromLatin1(kArchiveFilter));
        if (!path.isEmpty())
            acceptWith(Choice::OpenPath, path);
    });
    connect(m_newProjectButton, &QPushButton::clicked, this,
            [this] { acceptWith(Choice::NewProject); });
    connect(m_emptyProjectButton, &QPushButton::clicked, this,
            [this] { acceptWith(Choice::Empty); });

    // 单击条目即选择该存档（参照 VS 启动窗口的最近列表交互）
    connect(m_recentList, &QListWidget::itemClicked, this, [this](QListWidgetItem* item) {
        acceptWith(Choice::OpenPath, item->data(Qt::UserRole).toString());
    });
    connect(m_searchEdit, &QLineEdit::textChanged, this,
            [this](const QString&) { rebuildList(); });

    rebuildList();
}

void StartDialog::acceptWith(Choice choice, const QString& path)
{
    m_choice = choice;
    m_selectedPath = path;
    accept();
}

QWidget* StartDialog::makeEntryRow(QPushButton*& buttonOut, const QString& objectName,
                                   const QString& title, const QString& description)
{
    auto* row = new QWidget(this);
    auto* layout = new QVBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    auto* button = new QPushButton(title, row);
    button->setObjectName(objectName);
    QFont buttonFont = button->font();
    buttonFont.setPointSize(buttonFont.pointSize() + 1);
    button->setFont(buttonFont);
    layout->addWidget(button);

    auto* descLabel = new QLabel(description, row);
    descLabel->setStyleSheet(QString::fromLatin1(kGrayTextStyle));
    layout->addWidget(descLabel);

    buttonOut = button;
    return row;
}

void StartDialog::rebuildList()
{
    const QString needle = m_searchEdit->text().trimmed();

    m_recentList->clear();
    for (const QString& path : m_allRecent) {
        const QString fileName = QFileInfo(path).fileName();
        // 搜索对文件名 + 完整路径做大小写不敏感包含过滤
        if (!needle.isEmpty()
            && !fileName.contains(needle, Qt::CaseInsensitive)
            && !path.contains(needle, Qt::CaseInsensitive))
            continue;
        auto* item = new QListWidgetItem(fileName + QStringLiteral("\n") + path,
                                         m_recentList);
        item->setData(Qt::UserRole, path);

        // 两行显示：文件名加粗 + 完整路径灰色小字（item 文本保留同内容供自测断言）
        auto* itemWidget = new QWidget(m_recentList);
        auto* itemLayout = new QVBoxLayout(itemWidget);
        itemLayout->setContentsMargins(8, 4, 8, 4);
        itemLayout->setSpacing(0);
        auto* nameLabel = new QLabel(fileName, itemWidget);
        QFont nameFont = nameLabel->font();
        nameFont.setBold(true);
        nameLabel->setFont(nameFont);
        auto* pathLabel = new QLabel(path, itemWidget);
        pathLabel->setStyleSheet(QString::fromLatin1(kGrayTextStyle));
        itemLayout->addWidget(nameLabel);
        itemLayout->addWidget(pathLabel);
        item->setSizeHint(itemWidget->sizeHint());
        m_recentList->setItemWidget(item, itemWidget);
    }

    if (m_recentList->count() > 0) {
        m_recentStack->setCurrentWidget(m_recentList);
        return;
    }
    m_emptyLabel->setText(m_allRecent.isEmpty()
                              ? QStringLiteral("暂无最近使用的存档，从右侧新建或打开")
                              : QStringLiteral("无匹配的最近存档"));
    m_recentStack->setCurrentWidget(m_emptyLabel);
}
