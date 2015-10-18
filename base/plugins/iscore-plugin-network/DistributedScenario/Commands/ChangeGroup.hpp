#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include "DistributedScenario/Group.hpp"

class ChangeGroup : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("NetworkControl", "ChangeGroup", "Change the group of an element")
    public:
        ChangeGroup(ObjectPath&& path, Id<Group> newGroup);

        void undo() const override;
        void redo() const override;

        virtual void serializeImpl(QDataStream & s) const override;
        virtual void deserializeImpl(QDataStream & s) override;

    private:
        ObjectPath m_path;
        Id<Group> m_newGroup, m_oldGroup;
};
