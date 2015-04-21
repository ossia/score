#pragma once
#include <QString>
#include <iscore/tools/IdentifiedObject.hpp>
#include "Repartition/client/Client.hpp"
#include <iscore/serialization/DataStreamVisitor.hpp>

class Group;

// Goes into the constraints, events, etc.
// TODO this needs to be a QObject in order to be able to be updated.
struct GroupMetadata
{
        friend QDataStream &operator<<(QDataStream &out, const GroupMetadata &myObj)
        {
            Serializer<DataStream> s{out.device()};
            s.readFrom(myObj.id);
            return out;
        }

        friend QDataStream &operator>>(QDataStream &in, GroupMetadata &myObj)
        {
            Deserializer<DataStream> s{in.device()};
            s.writeTo(myObj.id);
            return in;
        }

    id_type<Group> id;
};
Q_DECLARE_METATYPE(GroupMetadata)

// Groups : registered in the session
// Permissions ? for now we will just have, for each constraint in a score,
// a chosen group.
// There is a "default" group that runs the constraint everywhere.
// The groups are part of the document plugin.
// Each client can be in a group (it will execute all the constraints that are part of this group).
class Group : public IdentifiedObject<Group>
{
        Q_OBJECT
        Q_PROPERTY(QString name READ name WRITE setName NOTIFY nameChanged)
    public:
        Group(QString name, id_type<Group> id, QObject* parent):
            IdentifiedObject<Group>{id, "Group", parent},
            m_name{name}
        {

        }

        QString name() const
        {
            return m_name;
        }

        void setName(QString arg)
        {
            if (m_name == arg)
                return;

            m_name = arg;
            emit nameChanged(arg);
        }

        void addClient(id_type<Client> clt)
        {
            m_executingClients.push_back(clt);
            emit clientAdded(clt);
        }

        void removeClient(id_type<Client> clt)
        {
            auto it = std::find(std::begin(m_executingClients), std::end(m_executingClients), clt);
            Q_ASSERT(it != std::end(m_executingClients));

            m_executingClients.erase(it);
            emit clientRemoved(clt);
        }

        const auto& clients() const { return m_executingClients; }

    signals:
        void nameChanged(QString arg);

        void clientAdded(id_type<Client>);
        void clientRemoved(id_type<Client>);

    private:
        QString m_name;
        id_type<Group> m_id {};

        std::vector<id_type<Client>> m_executingClients;
};


// TODO Metadata GUI (combobox)
// TODO Commands (changeEventGroup, changeConstraintGroup...)
// TODO add client to group

class GroupManager : public IdentifiedObject<GroupManager>
{
        Q_OBJECT
    public:
        GroupManager(QObject* parent):
            IdentifiedObject<GroupManager>{id_type<GroupManager>{0}, "GroupManager", parent}
        {

        }

        void addGroup(Group* group)
        {
            m_groups.push_back(group);
            emit groupsChanged();
        }

        void removeGroup(id_type<Group> group)
        {
            qDebug() << Q_FUNC_INFO << "TODO";
            emit groupsChanged();
        }

        const auto& groups() const
        { return m_groups; }

        auto group(const id_type<Group>& id) const
        { return *std::find(std::begin(m_groups), std::end(m_groups), id); }

    signals:
        void groupsChanged();

    private:
        std::vector<Group*> m_groups;
};
