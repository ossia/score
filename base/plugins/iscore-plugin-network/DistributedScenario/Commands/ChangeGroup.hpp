#pragma once
#include <DistributedScenario/Commands/DistributedScenarioCommandFactory.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include "DistributedScenario/Group.hpp"

class ChangeGroup : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(DistributedScenarioCommandFactoryName(), ChangeGroup, "Change the group of an element")

    public:
        ChangeGroup(ObjectPath&& path, Id<Group> newGroup);

        void undo() const override;
        void redo() const override;

        void serializeImpl(DataStreamInput & s) const override;
        void deserializeImpl(DataStreamOutput & s) override;

    private:
        ObjectPath m_path;
        Id<Group> m_newGroup, m_oldGroup;
};
