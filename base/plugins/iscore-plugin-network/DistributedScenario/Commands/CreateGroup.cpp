#include "CreateGroup.hpp"

#include "DistributedScenario/Group.hpp"
#include "DistributedScenario/GroupManager.hpp"
#include <iscore/tools/SettableIdentifierGeneration.hpp>


CreateGroup::CreateGroup(ObjectPath&& groupMgrPath, QString groupName):
    iscore::SerializableCommand{"NetworkControl", "CreateGroup", "CreateGroup_desc"},
    m_path{groupMgrPath},
    m_name{groupName}
{
    auto& mgr = m_path.find<GroupManager>();
    m_newGroupId = getStrongId(mgr.groups());
}

void CreateGroup::undo() const
{
    m_path.find<GroupManager>().removeGroup(m_newGroupId);
}

void CreateGroup::redo() const
{
    auto& mgr = m_path.find<GroupManager>();
    mgr.addGroup(new Group{m_name, m_newGroupId, &mgr});
}

void CreateGroup::serializeImpl(QDataStream& s) const
{
    s << m_path << m_name << m_newGroupId;
}

void CreateGroup::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_name >> m_newGroupId;
}
