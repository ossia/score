#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <iscore/tools/ObjectPath.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include "DistributedScenario/Group.hpp"

template<typename T>
class ChangeGroup : public iscore::SerializableCommand
{
    public:
        ChangeGroup(ObjectPath&& path, id_type<Group> newGroup, QString commandName, QString description):
            iscore::SerializableCommand{"NetworkControl", commandName, description},
            m_path{path},
            m_newGroup{newGroup}
        {
            for(const QVariant& elt : m_path.find<T>()->metadata.pluginMetadata())
            {
                if(elt.canConvert<GroupMetadata>())
                {
                    m_oldGroup = elt.value<GroupMetadata>().m_id;
                    break;
                }
            }
        }

        virtual void undo() override
        {
            m_path.find<T>()->metadata.updatePluginMetadata(GroupMetadata{m_oldGroup});
        }

        virtual void redo() override
        {
            m_path.find<T>()->metadata.updatePluginMetadata(GroupMetadata{m_newGroup});
        }

        virtual bool mergeWith(const iscore::Command*) override
        {
            return false;
        }

        virtual void serializeImpl(QDataStream & s) const override
        {
            s << m_path << m_oldGroup << m_newGroup;
        }

        virtual void deserializeImpl(QDataStream & s) override
        {
            s >> m_path >> m_oldGroup >> m_newGroup;
        }

    private:
        ObjectPath m_path;
        id_type<Group> m_oldGroup, m_newGroup;
};
