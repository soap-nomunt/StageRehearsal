#pragma once

// 统一设置窗口（阶段7，VSCode 设置页风格）：合并原"舞台设置"与"偏好设置"两个模态
// 对话框为非模态单例窗口——顶部搜索框（按关键词在全部分类的设置行中过滤）、
// 左侧分类导航（舞台/画布/时间轴）、右侧设置列表（每行 = 加粗标题 + 灰色描述
// 小字 + 控件）。无 OK/Cancel：所有改动即时生效；窗口经标题栏关闭按钮/Esc 关闭
// （QDialog 非模态下 reject 即隐藏——只隐藏不销毁，下次打开复用同一实例状态）。
//
// 右侧分页选型：QStackedWidget 分页（而非分组滚动）。设置项共 7 行，"点击分类
// 切换显示该分类设置行"用分页是明确可测的状态（currentWidget），无需滚动定位
// 逻辑；搜索时右侧切到专用"搜索结果"页，命中行重挂进该页（控件全局只存在一份，
// 重挂而非复制，objectName 与行为保持唯一），天然满足"搜索忽略分类分页限制"；
// 清空搜索后按行创建顺序挂回原分类页，并回到左侧导航当前选中的分类页。
//
// 数值控件提交时机：不做 valueChanged 即时提交（避免一次连续调节产生一串提交），
// editingFinished（回车/失焦）时若与当前 StageData 不同才调 EditorCore::setStage；
// fileDataChanged 时回显刷新控件（新建/打开文件后显示值同步），刷新屏蔽信号防回环。
//
// 语义分界（保持既有约定）：舞台宽/深/中心点偏移是文件数据（随存档保存、参与
// dirty 跟踪），经 EditorCore::setStage 直改直提交、不入撤销栈（撤销栈只覆盖
// 主视口/Hierarchy/Inspector/时间轴中的可操作对象，舞台只能在本窗口调整，不在
// 其列）；舞台颜色/网格吸附/显示网格/打帧询问是软件自身设置（AppSettings，
// QSettings 持久化），不进文件、不弄脏 dirty、不入撤销栈。

#include <QColor>
#include <QDialog>
#include <QList>
#include <QString>

class EditorCore;
class RuntimeData;
class QCheckBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QStackedWidget;
class QVBoxLayout;

class SettingsWindow : public QDialog {
    Q_OBJECT
public:
    // 分类：与左侧导航行号一一对应（菜单入口据此定位）
    enum class Category { Stage = 0, Canvas = 1, Timeline = 2 };

    SettingsWindow(RuntimeData* runtimeData, EditorCore* editorCore, QWidget* parent = nullptr);

    // 菜单入口统一走这里：清掉上次搜索、导航定位到目标分类，show/raise/activate
    // 复用已显示窗口（单例由 MainWindow 持有，懒创建）
    void openCategory(Category category);

signals:
    void stageColorChanged();      // 舞台颜色改动后立即发出，供主窗口重绘场景
    void gridVisibilityChanged();  // 显示网格开关切换后发出，供主窗口重绘场景
    void gridAppearanceChanged();  // 网格间距改动后发出，供主窗口重绘场景（吸附距离无需重绘）

private:
    // 一个设置行 = 行控件 + 所属分类页布局 + 搜索匹配用的标题/描述文本
    struct SettingRow {
        QWidget* widget = nullptr;
        QVBoxLayout* homeLayout = nullptr; // 所属分类页布局（行插到其末尾 stretch 前）
        QString title;
        QString description;
    };

    QWidget* createPage(const QString& objectName, QVBoxLayout*& layoutOut);
    // 创建 VSCode 风格三件套行并登记进 m_rows（调用方负责插入分类页）
    QWidget* createRow(QVBoxLayout* homeLayout, const QString& title,
                       const QString& description, QWidget* control);
    QDoubleSpinBox* makeStageSpin(double minimum, double maximum,
                                  const QString& objectName);

    void commitStageSpins();    // editingFinished：与当前 StageData 不同才 setStage
    void refreshStageControls(); // fileDataChanged 回显（屏蔽信号防回环）
    void applySearchFilter();   // 搜索过滤 / 清空恢复
    void updateColorSwatch();   // 色块按钮底色与色值文本随当前设置刷新

    RuntimeData* m_runtimeData = nullptr; // 不拥有
    EditorCore* m_editorCore = nullptr;   // 不拥有

    QLineEdit* m_searchEdit = nullptr;     // settingsSearchEdit
    QListWidget* m_navTree = nullptr;      // settingsNavTree（平铺三项，用 QListWidget）
    QStackedWidget* m_pageStack = nullptr; // settingsPageStack
    QWidget* m_searchPage = nullptr;       // settingsSearchPage（搜索结果专用页）
    QLabel* m_noMatchLabel = nullptr;      // settingsNoMatchLabel（无匹配占位提示）
    QVBoxLayout* m_searchLayout = nullptr;

    QDoubleSpinBox* m_widthSpin = nullptr;  // stageWidthSpin
    QDoubleSpinBox* m_depthSpin = nullptr;  // stageDepthSpin
    QDoubleSpinBox* m_offsetSpin = nullptr; // stageOffsetSpin
    QPushButton* m_colorButton = nullptr;   // stageColorButton
    QCheckBox* m_gridVisibleCheck = nullptr;    // gridVisibleCheck
    QCheckBox* m_gridSnapCheck = nullptr;       // gridSnapCheck
    QCheckBox* m_keyframePromptCheck = nullptr; // keyframePromptCheck
    QDoubleSpinBox* m_gridSizeSpin = nullptr;   // gridSizeSpin（网格间距，即改即生效）
    QDoubleSpinBox* m_snapRangeSpin = nullptr;  // snapRangeSpin（吸附距离，即改即生效）

    QList<SettingRow> m_rows; // 全部设置行（创建顺序 = 分类页内顺序 = 搜索页内顺序）
};
