#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <iscore/tools/IdentifiedObject.hpp>

class Client;
class Group;
class AddClientToGroup : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("AddClientToGroup", "AddClientToGroup")
    public:
        ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(AddClientToGroup, "NetworkControl")
        AddClientToGroup(ObjectPath&& groupMgrPath,
                         Id<Client> client,
                         Id<Group> group);

        virtual void undo() override;
        virtual void redo() override;

        virtual void serializeImpl(QDataStream & s) const override;
        virtual void deserializeImpl(QDataStream & s) override;

    private:
        ObjectPath m_path;
        Id<Client> m_client;
        Id<Group> m_group;
};
