#pragma once

// 编辑命令：可操作对象一切数据修改的统一载体。
// 命令只操作 FileData 内存副本，由 EditorCore::executeCommand 统一 apply 后
// 经 RuntimeData::commitFileData 提交回数据层；阶段4 撤销栈（UndoStack）
// 直接复用本文件：提交成功的命令入栈，undo/redo 走同一 commitFileData 路径。
// 唯一例外：舞台尺寸修改（阶段7 起）不构造命令、不入撤销栈——舞台不在
// 主视口/Hierarchy/Inspector/时间轴中操作，撤销栈只覆盖这四个窗口的
// 可操作对象；EditorCore::setStage 校验后直接 commitFileData 提交
// （SetStageCommand 已随之移除）。
//
// 约定：apply 首次执行时从 FileData 捕获旧值供 revert（对 redo 幂等）。

#include "data/DataTypes.h"

#include <QColor>
#include <QList>
#include <QPair>
#include <QString>

class Command {
public:
    virtual ~Command() = default;
    virtual void apply(FileData& data) = 0;
    virtual void revert(FileData& data) = 0;
};

// 单个对象的变换目标值（id + 新变换）。
struct ObjectTransform {
    QString id;
    Transform value;
};

// 变换替换：拖拽移动/方向键微调/旋转/Inspector 位置朝向修改共用。
// apply 首次执行时逐个捕获旧变换，revert 原样写回。
class SetTransformsCommand : public Command {
public:
    explicit SetTransformsCommand(QList<ObjectTransform> updates);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    QList<ObjectTransform> m_updates;
    QList<Transform> m_previous; // 与 m_updates 对齐，apply 首次执行时捕获
};

// 创建/粘贴：整批追加对象（id、唯一名、颜色由 EditorCore 预先生成好）。
class AddObjectsCommand : public Command {
public:
    explicit AddObjectsCommand(QList<ObjectData> objects);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    QList<ObjectData> m_objects;
};

// 删除：按 id 批量移除，级联删除整棵子树（含所有后代）；
// apply 首次执行时记录每个被删对象的 (原下标, 数据)，revert 原位插回。
class RemoveObjectsCommand : public Command {
public:
    explicit RemoveObjectsCommand(QList<QString> ids);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    QList<QString> m_ids;
    QList<QPair<int, ObjectData>> m_removed; // (原下标, 对象数据)，下标升序；含级联后代
    QList<PathData> m_oldPaths;              // 阶段12：对象删除级联路径的快照（整条列表）
    bool m_captured = false;
};

// 层级整理：拖拽改变 parentId，并按"列表顺序 = Hierarchy 深度优先显示顺序"
// 不变式重排列表（子树整体移动、保持连续）。
// insertIndex 语义：移除被移动子树后的剩余列表中，在 newParentId 的直系子对象
// 序列里的插入下标（>= 子对象数 = 排到该行子树末尾；newParentId 空串 = 顶层）。
// 环校验在 EditorCore 入口完成，命令本身不校验。
class ReparentObjectsCommand : public Command {
public:
    ReparentObjectsCommand(QList<QString> ids, QString newParentId, int insertIndex);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    QList<QString> m_ids;
    QString m_newParentId;
    int m_insertIndex;
    // apply 首次执行时捕获完整列表（每个 id 的旧 parentId 与原位置），revert 整体还原
    QList<ObjectData> m_previousObjects;
    bool m_captured = false;
};

// 改名（唯一性校验在 EditorCore 入口完成，命令本身不校验）。
class RenameObjectCommand : public Command {
public:
    RenameObjectCommand(QString id, QString newName);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    QString m_id;
    QString m_newName;
    QString m_oldName;
    bool m_captured = false;
};

class SetObjectColorCommand : public Command {
public:
    SetObjectColorCommand(QString id, QColor newColor);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    QString m_id;
    QColor m_newColor;
    QColor m_oldColor;
    bool m_captured = false;
};

// 仅屏风：修改大小。
class SetScreenSizeCommand : public Command {
public:
    SetScreenSizeCommand(QString id, QSizeF newSize);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    QString m_id;
    QSizeF m_newSize;
    QSizeF m_oldSize;
    bool m_captured = false;
};

// 仅道具（阶段13）：修改缩放（scaleX/scaleY 两值一次提交；xy 绑定语义由
// Inspector 组装——绑定后一方改动另一方同步增减同一增量，此处只落两值）。
class SetPropScaleCommand : public Command {
public:
    SetPropScaleCommand(QString id, double scaleX, double scaleY);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    QString m_id;
    double m_scaleX;
    double m_scaleY;
    double m_oldScaleX = 1.0;
    double m_oldScaleY = 1.0;
    bool m_captured = false;
};

// 仅道具（阶段13）：切换可供站立（standable=false 的道具阻挡路径编辑，任务3）
class SetPropStandableCommand : public Command {
public:
    SetPropStandableCommand(QString id, bool standable);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    QString m_id;
    bool m_standable;
    bool m_oldStandable = true;
    bool m_captured = false;
};

// 仅道具（阶段13 任务2）：形状编辑器确定时一次性替换形状（快照旧 shape；
// 写入前顶点绕向归一为逆时针）
class SetPropShapeCommand : public Command {
public:
    SetPropShapeCommand(QString id, PropShape shape);

    void apply(FileData& data) override;
    void revert(FileData& data) override;

private:
    QString m_id;
    PropShape m_shape;
    std::optional<PropShape> m_oldShape; // 缺省 = 默认矩形
    bool m_captured = false;
};
