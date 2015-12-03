
#include <algorithm>

#include "DistributedScenario/Group.hpp"
#include "DistributedScenario/GroupManager.hpp"
#include "RemoveClientFromGroup.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/tools/ObjectPath.hpp>


RemoveClientFromGroup::RemoveClientFromGroup(ObjectPath&& groupMgrPath, Id<Client> client, Id<Group> group):
    m_path{std::move(groupMgrPath)},
    m_client{client},
    m_group{group}
{
}

void RemoveClientFromGroup::undo() const
{
    m_path.find<GroupManager>().group(m_group)->addClient(m_client);
}

void RemoveClientFromGroup::redo() const
{
    m_path.find<GroupManager>().group(m_group)->removeClient(m_client);
}

void RemoveClientFromGroup::serializeImpl(DataStreamInput& s) const
{
    s << m_path << m_client << m_group;
}

void RemoveClientFromGroup::deserializeImpl(DataStreamOutput& s)
{
    s >> m_path >> m_client >> m_group;
}
