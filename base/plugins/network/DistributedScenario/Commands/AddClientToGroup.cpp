#include "AddClientToGroup.hpp"

#include "DistributedScenario/Group.hpp"
#include "DistributedScenario/GroupManager.hpp"


AddClientToGroup::AddClientToGroup(ObjectPath&& groupMgrPath,
                                   id_type<Client> client,
                                   id_type<Group> group):
    iscore::SerializableCommand{"NetworkControl", "AddClientToGroup", "AddClientToGroup_desc"},
    m_path{std::move(groupMgrPath)},
    m_client{client},
    m_group{group}
{
}

void AddClientToGroup::undo()
{
    m_path.find<GroupManager>()->group(m_group)->removeClient(m_client);
}

void AddClientToGroup::redo()
{
    m_path.find<GroupManager>()->group(m_group)->addClient(m_client);
}

bool AddClientToGroup::mergeWith(const iscore::Command*)
{
    return false;
}

void AddClientToGroup::serializeImpl(QDataStream& s) const
{
    s << m_path << m_client << m_group;
}

void AddClientToGroup::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_client >> m_group;
}
