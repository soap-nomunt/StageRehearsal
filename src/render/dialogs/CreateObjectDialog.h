#pragma once

// 创建对象对话框：选择对象类型（演员/屏风/空对象/舞台道具（阶段13 起启用））。
// 位置由调用方决定：视口右键位置 / Hierarchy 为 (0,0)（空对象忽略位置、落顶层）。

#include "data/DataTypes.h"

#include <QDialog>

class QRadioButton;

class CreateObjectDialog : public QDialog {
    Q_OBJECT
public:
    explicit CreateObjectDialog(QWidget* parent = nullptr);

    ObjectType selectedType() const;

private:
    QRadioButton* m_actorRadio = nullptr;
    QRadioButton* m_screenRadio = nullptr;
    QRadioButton* m_groupRadio = nullptr;
    QRadioButton* m_propRadio = nullptr;
};
