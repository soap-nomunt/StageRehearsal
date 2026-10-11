#pragma once

// 撤销栈（阶段4）：持有已执行命令，支持撤销/重做。
// 命令入栈点唯一：EditorCore::executeCommand 提交成功时 push。
// undo/redo 只在 FileData 内存副本上 revert/apply，提交回数据层
// （RuntimeData::commitFileData）由 EditorCore 完成，保证渲染层重建
// 与选中剔除行为与正向命令一致。
// 栈为纯内存态：不入存档；新建/打开文件时由 EditorCore 清空。
// 容量不设上限：命令只持有少量值类型快照（变换/对象数据/旧值），
// 单条开销百字节级，排练编排场景下内存可忽略。

#include "Commands.h"

#include <memory>
#include <vector>

class UndoStack {
public:
    // 新命令入栈（取得所有权），并清空 redo 区
    void push(std::unique_ptr<Command> command);

    // 撤销栈顶命令：revert 到 data；无可撤销时返回 false 且不改 data
    bool undo(FileData& data);
    // 重做：apply 回 data；无可重做时返回 false 且不改 data
    bool redo(FileData& data);

    bool canUndo() const { return !m_undoables.empty(); }
    bool canRedo() const { return !m_redoables.empty(); }

    void clear();

private:
    std::vector<std::unique_ptr<Command>> m_undoables;
    std::vector<std::unique_ptr<Command>> m_redoables;
};
