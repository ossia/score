#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <iscore/tools/IdentifiedObject.hpp>

class Group;
class CreateGroup : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL("CreateGroup", "CreateGroup")
    public:
        ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(CreateGroup, "NetworkControl")
        CreateGroup(ObjectPath&& groupMgrPath, QString groupName);

        virtual void undo() override;
        virtual void redo() override;

        virtual void serializeImpl(QDataStream & s) const override;
        virtual void deserializeImpl(QDataStream & s) override;

    private:
        ObjectPath m_path;
        QString m_name;
        id_type<Group> m_newGroupId;
};
