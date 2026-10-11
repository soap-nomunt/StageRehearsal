#pragma once

// 打帧询问弹窗（需求第 20 条，渲染层实现；业务层 EditorCore 只在需要询问时
// 经 KeyframeConfirmCallback 调到这里）。含"不再提示"勾选：勾选且选"是"时
// 写入 AppSettings::setKeyframePromptSuppressed(true)，之后业务层直接打帧不再询问。

#include "data/AppSettings.h"

#include <QCheckBox>
#include <QMessageBox>
#include <QPushButton>

namespace KeyframePrompt {

// 返回 true = 用户选"是"（打帧/更新）；false = 选"否"（调用方零变更并回弹显示）
inline bool confirmAtCursor(QWidget* parent, AppSettings& settings, double t)
{
    QMessageBox box(QMessageBox::Question, QStringLiteral("打帧确认"),
                    QStringLiteral("当前时刻 %1 s 该对象没有关键帧，是否在此打帧？")
                        .arg(t, 0, 'f', 2),
                    QMessageBox::Yes | QMessageBox::No, parent);
    box.button(QMessageBox::Yes)->setText(QStringLiteral("是"));
    box.button(QMessageBox::No)->setText(QStringLiteral("否"));
    box.setCheckBox(new QCheckBox(QStringLiteral("不再提示"), &box));
    const bool yes = box.exec() == QMessageBox::Yes;
    if (yes && box.checkBox() && box.checkBox()->isChecked())
        settings.setKeyframePromptSuppressed(true);
    return yes;
}

} // namespace KeyframePrompt
