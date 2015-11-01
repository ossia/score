#pragma once
#include <DistributedScenario/Commands/DistributedScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <iscore/tools/IdentifiedObject.hpp>

class Client;
class Group;
class RemoveClientFromGroup : public iscore::SerializableCommand
{
        ISCORE_SERIALIZABLE_COMMAND_DECL(
                DistributedScenarioCommandFactoryName(),
                RemoveClientFromGroup,
                "RemoveClientFromGroup")

    public:
        RemoveClientFromGroup(
                ObjectPath&& groupMgrPath,
                Id<Client> client,
                Id<Group> group);

        void undo() const override;
        void redo() const override;

        virtual void serializeImpl(QDataStream & s) const override;
        virtual void deserializeImpl(QDataStream & s) override;

    private:
        ObjectPath m_path;
        Id<Client> m_client;
        Id<Group> m_group;
};
