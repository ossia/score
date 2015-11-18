#pragma once
#include <DistributedScenario/Commands/DistributedScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <iscore/tools/IdentifiedObject.hpp>

class Client;
class Group;
class AddClientToGroup : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(
                DistributedScenarioCommandFactoryName(),
                AddClientToGroup,
                "AddClientToGroup")
    public:
        AddClientToGroup(ObjectPath&& groupMgrPath,
                         Id<Client> client,
                         Id<Group> group);

        void undo() const override;
        void redo() const override;

        void serializeImpl(DataStreamInput & s) const override;
        void deserializeImpl(DataStreamOutput & s) override;

    private:
        ObjectPath m_path;
        Id<Client> m_client;
        Id<Group> m_group;
};
