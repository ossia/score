#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include "DistributedScenario/Group.hpp"

class CreateGroup : public iscore::SerializableCommand
{
    public:
        CreateGroup(ObjectPath&& groupMgrPath, QString groupName):
            iscore::SerializableCommand{"NetworkControl", "CreateGroup", "CreateGroup_desc"},
            m_path{groupMgrPath},
            m_name{groupName}
        {
            auto mgr = m_path.find<GroupManager>();
            m_newGroupId = getStrongId(mgr->groups());
        }

        virtual void undo() override
        {
            m_path.find<GroupManager>()->removeGroup(m_newGroupId);
        }

        virtual void redo() override
        {
            auto mgr = m_path.find<GroupManager>();
            mgr->addGroup(new Group{m_name, m_newGroupId, mgr});
        }

        virtual bool mergeWith(const iscore::Command*) override
        {
            return false;
        }

        virtual void serializeImpl(QDataStream & s) const override
        {
            s << m_path << m_name << m_newGroupId;
        }

        virtual void deserializeImpl(QDataStream & s) override
        {
            s >> m_path >> m_name >> m_newGroupId;
        }

    private:
        ObjectPath m_path;
        QString m_name;
        id_type<Group> m_newGroupId;
};
