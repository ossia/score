#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include "DistributedScenario/Group.hpp"

class ChangeGroup : public iscore::SerializableCommand
{
    public:
        ChangeGroup(ObjectPath&& path, id_type<Group> newGroup);

        virtual void undo() override;
        virtual void redo() override;

        virtual bool mergeWith(const iscore::Command*) override;

        virtual void serializeImpl(QDataStream & s) const override;
        virtual void deserializeImpl(QDataStream & s) override;

    private:
        ObjectPath m_path;
        id_type<Group> m_newGroup, m_oldGroup;
};
