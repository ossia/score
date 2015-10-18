#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <iscore/tools/IdentifiedObject.hpp>

class Group;
class CreateGroup : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("NetworkControl", CreateGroup, "CreateGroup")
    public:
        CreateGroup(ObjectPath&& groupMgrPath, QString groupName);

        void undo() const override;
        void redo() const override;

        virtual void serializeImpl(QDataStream & s) const override;
        virtual void deserializeImpl(QDataStream & s) override;

    private:
        ObjectPath m_path;
        QString m_name;
        Id<Group> m_newGroupId;
};
