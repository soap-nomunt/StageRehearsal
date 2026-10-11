#include "UndoStack.h"

void UndoStack::push(std::unique_ptr<Command> command)
{
    if (!command)
        return;
    m_redoables.clear(); // 新命令使 redo 区失效
    m_undoables.push_back(std::move(command));
}

bool UndoStack::undo(FileData& data)
{
    if (m_undoables.empty())
        return false;
    std::unique_ptr<Command> command = std::move(m_undoables.back());
    m_undoables.pop_back();
    command->revert(data);
    m_redoables.push_back(std::move(command)); // undo 后 redo 区保留可重做
    return true;
}

bool UndoStack::redo(FileData& data)
{
    if (m_redoables.empty())
        return false;
    std::unique_ptr<Command> command = std::move(m_redoables.back());
    m_redoables.pop_back();
    command->apply(data); // apply 对 redo 幂等（旧值已捕获）
    m_undoables.push_back(std::move(command));
    return true;
}

void UndoStack::clear()
{
    m_undoables.clear();
    m_redoables.clear();
}
