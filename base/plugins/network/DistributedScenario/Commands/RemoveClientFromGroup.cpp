#include "RemoveClientFromGroup.hpp"

#include "DistributedScenario/Group.hpp"
#include "DistributedScenario/GroupManager.hpp"


RemoveClientFromGroup::RemoveClientFromGroup(ObjectPath&& groupMgrPath, id_type<Client> client, id_type<Group> group):
    iscore::SerializableCommand{"NetworkControl", "RemoveClientFromGroup", "RemoveClientFromGroup_desc"},
    m_path{std::move(groupMgrPath)},
    m_client{client},
    m_group{group}
{
}

void RemoveClientFromGroup::undo()
{
    m_path.find<GroupManager>()->group(m_group)->addClient(m_client);
}

void RemoveClientFromGroup::redo()
{
    m_path.find<GroupManager>()->group(m_group)->removeClient(m_client);
}

bool RemoveClientFromGroup::mergeWith(const iscore::Command*)
{
    return false;
}

void RemoveClientFromGroup::serializeImpl(QDataStream& s) const
{
    s << m_path << m_client << m_group;
}

void RemoveClientFromGroup::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_client >> m_group;
}
