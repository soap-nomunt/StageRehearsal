#include "SettingsWindow.h"

#include "business/EditorCore.h"
#include "data/AppSettings.h"
#include "data/RuntimeData.h"

#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QColorDialog>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QVBoxLayout>

SettingsWindow::SettingsWindow(RuntimeData* runtimeData, EditorCore* editorCore,
                               QWidget* parent)
    : QDialog(parent)
    , m_runtimeData(runtimeData)
    , m_editorCore(editorCore)
{
    setWindowTitle(QStringLiteral("设置"));
    resize(760, 520);
    setMinimumSize(520, 360);

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(12, 12, 12, 12);
    rootLayout->setSpacing(8);

    // 顶部搜索框：按关键词过滤（标题 + 描述，大小写不敏感包含匹配）
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setObjectName(QStringLiteral("settingsSearchEdit"));
    m_searchEdit->setPlaceholderText(QStringLiteral("搜索设置"));
    m_searchEdit->setClearButtonEnabled(true);
    rootLayout->addWidget(m_searchEdit);

    // 主体：左分类导航（固定宽度）+ 右设置分页。选 QHBoxLayout 而非 QSplitter：
    // 设置项仅 7 行，无需用户拖拽分栏，固定宽度导航更贴近 VSCode 设置页观感
    auto* bodyLayout = new QHBoxLayout;
    bodyLayout->setSpacing(12);
    rootLayout->addLayout(bodyLayout, 1);

    m_navTree = new QListWidget(this);
    m_navTree->setObjectName(QStringLiteral("settingsNavTree"));
    m_navTree->setFrameShape(QFrame::NoFrame);
    m_navTree->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_navTree->setFixedWidth(120);
    m_navTree->addItem(QStringLiteral("舞台"));
    m_navTree->addItem(QStringLiteral("画布"));
    m_navTree->addItem(QStringLiteral("时间轴"));
    bodyLayout->addWidget(m_navTree);

    m_pageStack = new QStackedWidget(this);
    m_pageStack->setObjectName(QStringLiteral("settingsPageStack"));
    bodyLayout->addWidget(m_pageStack, 1);

    QVBoxLayout* stageLayout = nullptr;
    QVBoxLayout* canvasLayout = nullptr;
    QVBoxLayout* timelineLayout = nullptr;
    createPage(QStringLiteral("settingsStagePage"), stageLayout);
    createPage(QStringLiteral("settingsCanvasPage"), canvasLayout);
    createPage(QStringLiteral("settingsTimelinePage"), timelineLayout);

    // 搜索结果专用页：占位提示 + 末尾 stretch，命中行重挂进 stretch 前
    m_searchPage = createPage(QStringLiteral("settingsSearchPage"), m_searchLayout);
    m_noMatchLabel = new QLabel(QStringLiteral("无匹配设置"), m_searchPage);
    m_noMatchLabel->setObjectName(QStringLiteral("settingsNoMatchLabel"));
    m_noMatchLabel->setStyleSheet(QStringLiteral("color: #888888;"));
    m_noMatchLabel->setVisible(false);
    m_searchLayout->insertWidget(0, m_noMatchLabel);

    AppSettings& settings = m_runtimeData->settings();

    // ---- 舞台分类（宽/深/中心点偏移 = 文件数据；舞台颜色 = 软件自身设置） ----
    m_widthSpin = makeStageSpin(1.0, 100000.0, QStringLiteral("stageWidthSpin"));
    createRow(stageLayout, QStringLiteral("舞台：宽度"),
              QStringLiteral("舞台左右方向的总宽度，单位 cm，取值 1～100000。"
                             "回车或失焦后生效并立即刷新视口。"
                             "舞台尺寸随存档保存（文件数据），不进入撤销历史。"),
              m_widthSpin);

    m_depthSpin = makeStageSpin(1.0, 100000.0, QStringLiteral("stageDepthSpin"));
    createRow(stageLayout, QStringLiteral("舞台：深度"),
              QStringLiteral("舞台纵深方向（里外）的总深度，单位 cm，取值 1～100000。"
                             "回车或失焦后生效。"),
              m_depthSpin);

    m_offsetSpin = makeStageSpin(-100000.0, 100000.0, QStringLiteral("stageOffsetSpin"));
    createRow(stageLayout, QStringLiteral("舞台：中心点偏移"),
              QStringLiteral("舞台左右对称中心相对原点（舞台最里边中点）的水平偏移，"
                             "单位 cm，可取负值，取值 -100000～100000。回车或失焦后生效。"),
              m_offsetSpin);

    // 舞台颜色：色块按钮弹 QColorDialog（Qt 弹窗而非原生，便于自测驱动）；
    // 选中立写 AppSettings 并发 stageColorChanged 供主窗口重绘
    m_colorButton = new QPushButton(this);
    m_colorButton->setObjectName(QStringLiteral("stageColorButton"));
    m_colorButton->setFixedSize(200, 28); // 与数值输入框同宽，行内控件对齐
    updateColorSwatch();
    connect(m_colorButton, &QPushButton::clicked, this, [this] {
        const QColor current = m_runtimeData->settings().stageColor();
        const QColor chosen = QColorDialog::getColor(current, this, QStringLiteral("舞台颜色"),
                                                     QColorDialog::DontUseNativeDialog);
        if (!chosen.isValid() || chosen == current)
            return;
        m_runtimeData->settings().setStageColor(chosen); // setter 即持久化
        updateColorSwatch();
        emit stageColorChanged();
    });
    createRow(stageLayout, QStringLiteral("舞台：颜色"),
              QStringLiteral("舞台矩形在画布上的填充色。这是软件自身设置，不随存档保存、"
                             "不影响文件的未保存状态；点击色块选择后立即生效并持久化。"),
              m_colorButton);

    // ---- 画布分类（显示网格/网格吸附/网格间距/吸附距离：即改即写 AppSettings 生效） ----
    m_gridVisibleCheck = new QCheckBox(this);
    m_gridVisibleCheck->setObjectName(QStringLiteral("gridVisibleCheck"));
    m_gridVisibleCheck->setChecked(settings.gridVisible());
    connect(m_gridVisibleCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_runtimeData->settings().setGridVisible(checked);
        emit gridVisibilityChanged(); // 网格可见性变化 → 场景重绘
    });
    createRow(canvasLayout, QStringLiteral("画布：显示网格"),
              QStringLiteral("在画布背景上绘制网格线（间距由“网格间距”设置决定）。"
                             "仅影响显示，不影响吸附；切换后视口立即重绘。"),
              m_gridVisibleCheck);

    m_gridSnapCheck = new QCheckBox(this);
    m_gridSnapCheck->setObjectName(QStringLiteral("gridSnapCheck"));
    m_gridSnapCheck->setChecked(settings.gridSnapEnabled());
    connect(m_gridSnapCheck, &QCheckBox::toggled, this,
            [this](bool checked) { m_runtimeData->settings().setGridSnapEnabled(checked); });
    createRow(canvasLayout, QStringLiteral("画布：网格吸附"),
              QStringLiteral("拖拽演员/屏风松手时吸附到附近的网格点"
                             "（吸附距离由“吸附距离”设置决定）。"),
              m_gridSnapCheck);

    // 网格间距/吸附距离是软件自身设置：数值即改即生效（写 QSettings 很便宜，
    // 不涉及文件数据，无提交风暴问题），网格间距变化需重绘视口 → gridAppearanceChanged
    const auto makeCanvasSpin = [this](double minimum, double maximum, double step,
                                       double value, const QString& objectName) {
        auto* spin = new QDoubleSpinBox(this);
        spin->setObjectName(objectName);
        spin->setRange(minimum, maximum);
        spin->setDecimals(1);
        spin->setSingleStep(step);
        spin->setSuffix(QStringLiteral(" cm"));
        spin->setFixedWidth(200);
        spin->setValue(value); // connect 之前回显，不产生误写
        return spin;
    };

    m_gridSizeSpin = makeCanvasSpin(10.0, 1000.0, 10.0, settings.gridSizeCm(),
                                    QStringLiteral("gridSizeSpin"));
    connect(m_gridSizeSpin, &QDoubleSpinBox::valueChanged, this, [this](double value) {
        m_runtimeData->settings().setGridSizeCm(value);
        emit gridAppearanceChanged();
    });
    createRow(canvasLayout, QStringLiteral("画布：网格间距"),
              QStringLiteral("网格线之间的间距，单位 cm，取值 10～1000。"
                             "改动立即生效并重绘视口。"),
              m_gridSizeSpin);

    m_snapRangeSpin = makeCanvasSpin(1.0, 200.0, 5.0, settings.snapRangeCm(),
                                     QStringLiteral("snapRangeSpin"));
    connect(m_snapRangeSpin, &QDoubleSpinBox::valueChanged, this,
            [this](double value) { m_runtimeData->settings().setSnapRangeCm(value); });
    createRow(canvasLayout, QStringLiteral("画布：吸附距离"),
              QStringLiteral("拖拽演员/屏风松手时，吸附到该距离范围内的最近网格点，"
                             "单位 cm，取值 1～200。仅在“网格吸附”开启时生效。"),
              m_snapRangeSpin);

    // ---- 时间轴分类（打帧询问：keyframePromptSuppressed 的反义展示） ----
    m_keyframePromptCheck = new QCheckBox(this);
    m_keyframePromptCheck->setObjectName(QStringLiteral("keyframePromptCheck"));
    m_keyframePromptCheck->setChecked(!settings.keyframePromptSuppressed()); // 勾选=每次询问
    connect(m_keyframePromptCheck, &QCheckBox::toggled, this, [this](bool checked) {
        m_runtimeData->settings().setKeyframePromptSuppressed(!checked);
    });
    createRow(timelineLayout, QStringLiteral("时间轴：打帧前询问我"),
              QStringLiteral("勾选：在游标处没有关键帧时修改对象位置/朝向，每次都会询问"
                             "是否打帧；取消勾选：不再询问、直接打帧。若曾在打帧弹窗中"
                             "勾选“不再提示”，在此重新勾选即可恢复询问。"),
              m_keyframePromptCheck);

    // 数值行：editingFinished（回车/失焦）才提交，一次连续调节不产生一串提交
    connect(m_widthSpin, &QAbstractSpinBox::editingFinished,
            this, &SettingsWindow::commitStageSpins);
    connect(m_depthSpin, &QAbstractSpinBox::editingFinished,
            this, &SettingsWindow::commitStageSpins);
    connect(m_offsetSpin, &QAbstractSpinBox::editingFinished,
            this, &SettingsWindow::commitStageSpins);

    // 新建/打开文件后回显同步；任何 fileDataChanged 都刷新（值相同则 setValue 无副作用）
    connect(m_runtimeData, &RuntimeData::fileDataChanged,
            this, &SettingsWindow::refreshStageControls);
    refreshStageControls();

    connect(m_searchEdit, &QLineEdit::textChanged,
            this, [this](const QString&) { applySearchFilter(); });
    connect(m_navTree, &QListWidget::currentRowChanged, this, [this](int row) {
        // 搜索中忽略分类分页：仅记录导航选择，清空搜索后回到该分类页
        if (row >= 0 && m_searchEdit->text().trimmed().isEmpty())
            m_pageStack->setCurrentIndex(row);
    });
    m_navTree->setCurrentRow(0);
}

void SettingsWindow::openCategory(Category category)
{
    m_searchEdit->clear(); // 清掉上次搜索：applySearchFilter 恢复分类页显示
    m_navTree->setCurrentRow(static_cast<int>(category));
    show();
    raise();
    activateWindow();
}

QWidget* SettingsWindow::createPage(const QString& objectName, QVBoxLayout*& layoutOut)
{
    auto* page = new QWidget(this);
    page->setObjectName(objectName);
    auto* layout = new QVBoxLayout(page);
    layout->setContentsMargins(8, 4, 8, 8);
    layout->setSpacing(4);
    layout->addStretch(); // 末尾 stretch 常驻：行一律插到它前面，重挂不破坏布局
    m_pageStack->addWidget(page);
    layoutOut = layout;
    return page;
}

QWidget* SettingsWindow::createRow(QVBoxLayout* homeLayout, const QString& title,
                                   const QString& description, QWidget* control)
{
    auto* row = new QWidget(this);
    auto* layout = new QVBoxLayout(row);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(3);

    auto* titleLabel = new QLabel(title, row);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    layout->addWidget(titleLabel);

    auto* descLabel = new QLabel(description, row);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet(QStringLiteral("color: #888888;")); // 灰色小字，深浅主题均可读
    layout->addWidget(descLabel);

    layout->addWidget(control);

    homeLayout->insertWidget(homeLayout->count() - 1, row); // stretch 前
    m_rows.append({ row, homeLayout, title, description });
    return row;
}

QDoubleSpinBox* SettingsWindow::makeStageSpin(double minimum, double maximum,
                                              const QString& objectName)
{
    auto* spin = new QDoubleSpinBox(this);
    spin->setObjectName(objectName);
    spin->setRange(minimum, maximum);
    spin->setDecimals(1);
    spin->setSingleStep(10.0);
    spin->setSuffix(QStringLiteral(" cm"));
    spin->setFixedWidth(200);
    return spin;
}

void SettingsWindow::commitStageSpins()
{
    const StageData current = m_runtimeData->fileData().stage;
    const StageData edited { m_widthSpin->value(), m_depthSpin->value(),
                             m_offsetSpin->value() };
    if (edited == current)
        return; // 回车/失焦但未改值：不打扰数据层
    // 直改直提交、不入撤销栈；spin 下限 1.0 使非法尺寸到不了业务层
    // （业务层仍做防御性拒绝，由 DataLayerSelfTest 覆盖）
    m_editorCore->setStage(edited);
}

void SettingsWindow::refreshStageControls()
{
    const StageData& stage = m_runtimeData->fileData().stage;
    const QSignalBlocker blockWidth(m_widthSpin);
    const QSignalBlocker blockDepth(m_depthSpin);
    const QSignalBlocker blockOffset(m_offsetSpin);
    m_widthSpin->setValue(stage.width);
    m_depthSpin->setValue(stage.depth);
    m_offsetSpin->setValue(stage.centerOffset);
}

void SettingsWindow::applySearchFilter()
{
    const QString needle = m_searchEdit->text().trimmed();
    if (needle.isEmpty()) {
        // 清空：按创建顺序挂回原分类页（相对顺序不变），回到导航当前分类页
        for (const SettingRow& row : m_rows) {
            row.homeLayout->insertWidget(row.homeLayout->count() - 1, row.widget);
            row.widget->setVisible(true);
        }
        m_noMatchLabel->setVisible(false);
        m_pageStack->setCurrentIndex(m_navTree->currentRow());
        return;
    }

    int matches = 0;
    for (const SettingRow& row : m_rows) {
        const bool hit = row.title.contains(needle, Qt::CaseInsensitive)
            || row.description.contains(needle, Qt::CaseInsensitive);
        if (hit) {
            // 命中行重挂进搜索结果页（控件只此一份，重挂而非复制）
            m_searchLayout->insertWidget(m_searchLayout->count() - 1, row.widget);
            row.widget->setVisible(true);
            ++matches;
        } else {
            row.widget->setVisible(false); // 留在原分类页但隐藏
        }
    }
    m_noMatchLabel->setVisible(matches == 0);
    m_pageStack->setCurrentWidget(m_searchPage);
}

void SettingsWindow::updateColorSwatch()
{
    const QColor color = m_runtimeData->settings().stageColor();
    m_colorButton->setText(color.name());
    m_colorButton->setStyleSheet(
        QStringLiteral("background-color: %1; color: %2;")
            .arg(color.name(),
                 color.lightness() < 128 ? QStringLiteral("white")
                                         : QStringLiteral("black")));
}
