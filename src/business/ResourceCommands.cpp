#include "ResourceCommands.h"

#include <algorithm>

AddResourcesCommand::AddResourcesCommand(QList<ResourceData> resources)
    : m_resources(std::move(resources))
{
}

void AddResourcesCommand::apply(FileData& data)
{
    data.resources.append(m_resources);
}

void AddResourcesCommand::revert(FileData& data)
{
    for (const ResourceData& resource : m_resources) {
        for (qsizetype i = 0; i < data.resources.size(); ++i) {
            if (data.resources.at(i).id == resource.id) {
                data.resources.removeAt(i);
                break;
            }
        }
    }
}

RemoveResourceCommand::RemoveResourceCommand(QString id)
    : m_id(std::move(id))
{
}

void RemoveResourceCommand::apply(FileData& data)
{
    if (!m_captured) {
        for (qsizetype i = 0; i < data.resources.size(); ++i) {
            if (data.resources.at(i).id == m_id) {
                m_index = static_cast<int>(i);
                m_removed = data.resources.at(i);
                m_captured = true;
                break;
            }
        }
    }
    for (qsizetype i = 0; i < data.resources.size(); ++i) {
        if (data.resources.at(i).id == m_id) {
            data.resources.removeAt(i);
            break;
        }
    }
}

void RemoveResourceCommand::revert(FileData& data)
{
    if (!m_captured)
        return;
    for (const ResourceData& resource : data.resources) {
        if (resource.id == m_id)
            return;
    }
    const int index = std::clamp(m_index, 0, static_cast<int>(data.resources.size()));
    data.resources.insert(index, m_removed);
}

RenameResourceCommand::RenameResourceCommand(QString id, QString newName)
    : m_id(std::move(id))
    , m_newName(std::move(newName))
{
}

void RenameResourceCommand::apply(FileData& data)
{
    for (ResourceData& resource : data.resources) {
        if (resource.id != m_id)
            continue;
        if (!m_captured) {
            m_oldName = resource.name;
            m_captured = true;
        }
        resource.name = m_newName;
        return;
    }
}

void RenameResourceCommand::revert(FileData& data)
{
    if (!m_captured)
        return;
    for (ResourceData& resource : data.resources) {
        if (resource.id == m_id) {
            resource.name = m_oldName;
            return;
        }
    }
}
