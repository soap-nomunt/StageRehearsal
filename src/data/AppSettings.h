#pragma once

// 软件自身数据：与文件数据（FileData）零交叉。
// 上半部分为编译期常量，下半部分为 QSettings 持久化的用户设置。

#include <QByteArray>
#include <QColor>
#include <QList>
#include <QSettings>
#include <QStringList>

class AppSettings {
public:
    AppSettings();

    // ---- 常量（单位 cm / 度） ----
    static constexpr double actorRadiusCm         = 30.0;  // 演员圆形半径
    static constexpr double screenDefaultWidthCm  = 100.0; // 屏风默认宽
    static constexpr double screenDefaultHeightCm = 10.0;  // 屏风默认高
    static constexpr double arrowKeyStepCm        = 10.0;  // 方向键微调步长
    static constexpr double rotationSnapDegrees   = 15.0;  // 拖拽朝向手柄的旋转吸附步长

    // 用户设置的默认值：仅作常量单独保存，用户改动后才作为设置写入 QSettings
    static constexpr bool defaultGridSnapEnabled         = true;  // 网格吸附默认开
    static constexpr bool defaultGridVisible             = true;  // 网格可见默认开
    static constexpr bool defaultKeyframePromptSuppressed = false; // “打帧提示不再提示”默认关
    static constexpr double defaultGridSizeCm            = 100.0; // 网格间距默认值
    static constexpr double defaultSnapRangeCm           = 20.0;  // 网格吸附距离默认值

    // 对象默认调色板（创建演员、屏风和道具时按顺序轮转取色）
    static QList<QColor> defaultPalette();

    // 舞台颜色默认值（浅灰：比网格线 #d3d3d3 深、比画布底色 #242424 浅）。
    // 默认仅作常量单独保存；用户在"舞台"菜单改色后才作为设置写入 QSettings。
    static QColor defaultStageColor();

    // ---- 用户设置（QSettings 持久化，setter 立即写入） ----
    bool gridSnapEnabled() const;                 // 网格吸附开关，默认开
    void setGridSnapEnabled(bool enabled);

    bool gridVisible() const;                     // 网格可见性开关，默认开（仅影响显示，不影响吸附）
    void setGridVisible(bool visible);

    bool keyframePromptSuppressed() const;        // “打帧提示不再提示”开关，默认关
    void setKeyframePromptSuppressed(bool suppressed);

    double gridSizeCm() const;                    // 网格间距（cm），默认 100
    void setGridSizeCm(double sizeCm);            // ≤ 0 防御性拒绝（除零保护）

    double snapRangeCm() const;                   // 网格吸附距离（cm），默认 20
    void setSnapRangeCm(double rangeCm);          // ≤ 0 防御性拒绝

    QByteArray windowGeometry() const;            // 窗口布局
    void setWindowGeometry(const QByteArray& geometry);

    QByteArray windowState() const;
    void setWindowState(const QByteArray& state);

    QColor stageColor() const;                        // 舞台颜色（默认 #a0a0a0 浅灰），画布底色固定深灰
    void setStageColor(const QColor& color);

    // ---- 最近使用列表（阶段8，启动页唯一入口） ----
    static constexpr int kMaxRecentFiles = 10;        // MRU 上限
    QStringList recentFiles() const;                  // 新 → 旧
    // 去重后置顶：Windows 路径按大小写不敏感比较（保留本次写入的写法），超限截断最旧
    void addRecentFile(const QString& path);
    void removeRecentFile(const QString& path);       // 按同一比较移除（含失效条目清理）

private:
    QSettings m_settings;
};
