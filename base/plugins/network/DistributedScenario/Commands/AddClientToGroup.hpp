#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include "DistributedScenario/GroupManager.hpp"

class AddClientToGroup : public iscore::SerializableCommand
{
    public:
        AddClientToGroup(ObjectPath&& groupMgrPath, id_type<Client> client, id_type<Group> group):
            iscore::SerializableCommand{"NetworkControl", "AddClientToGroup", "AddClientToGroup_desc"},
            m_path{std::move(groupMgrPath)},
            m_client{client},
            m_group{group}
        {
        }

        virtual void undo() override
        {
            m_path.find<GroupManager>()->group(m_group)->removeClient(m_client);
        }

        virtual void redo() override
        {
            m_path.find<GroupManager>()->group(m_group)->addClient(m_client);
        }

        virtual bool mergeWith(const iscore::Command*) override
        {
            return false;
        }

        virtual void serializeImpl(QDataStream & s) const override
        {
            s << m_path << m_client << m_group;
        }

        virtual void deserializeImpl(QDataStream & s) override
        {
            s >> m_path >> m_client >> m_group;
        }

    private:
        ObjectPath m_path;
        id_type<Client> m_client;
        id_type<Group> m_group;
};
