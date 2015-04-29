#include "CreateGroup.hpp"

#include "DistributedScenario/Group.hpp"
#include "DistributedScenario/GroupManager.hpp"


CreateGroup::CreateGroup(ObjectPath&& groupMgrPath, QString groupName):
    iscore::SerializableCommand{"NetworkControl", "CreateGroup", "CreateGroup_desc"},
    m_path{groupMgrPath},
    m_name{groupName}
{
    auto mgr = m_path.find<GroupManager>();
    m_newGroupId = getStrongId(mgr->groups());
}

void CreateGroup::undo()
{
    m_path.find<GroupManager>()->removeGroup(m_newGroupId);
}

void CreateGroup::redo()
{
    auto mgr = m_path.find<GroupManager>();
    mgr->addGroup(new Group{m_name, m_newGroupId, mgr});
}

bool CreateGroup::mergeWith(const iscore::Command*)
{
    return false;
}

void CreateGroup::serializeImpl(QDataStream& s) const
{
    s << m_path << m_name << m_newGroupId;
}

void CreateGroup::deserializeImpl(QDataStream& s)
{
    s >> m_path >> m_name >> m_newGroupId;
}
