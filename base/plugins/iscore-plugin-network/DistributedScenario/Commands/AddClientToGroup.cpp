#include "AddClientToGroup.hpp"

#include "DistributedScenario/Group.hpp"
#include "DistributedScenario/GroupManager.hpp"


AddClientToGroup::AddClientToGroup(ObjectPath&& groupMgrPath,
                                   Id<Client> client,
                                   Id<Group> group):
    iscore::SerializableCommand{factoryName(), commandName(), description()},
    m_path{std::move(groupMgrPath)},
    m_client{client},
    m_group{group}
{
}

void AddClientToGroup::undo() const
{
    m_path.find<GroupManager>().group(m_group)->removeClient(m_client);
}

void AddClientToGroup::redo() const
{
    m_path.find<GroupManager>().group(m_group)->addClient(m_client);
}

void AddClientToGroup::serializeImpl(QDataStream& s) const
{
    s << m_path << m_client << m_group;
}

void AddClientToGroup::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_client >> m_group;
}
