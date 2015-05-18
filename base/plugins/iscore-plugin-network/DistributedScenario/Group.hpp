#pragma once
#include <QString>
#include <iscore/tools/IdentifiedObject.hpp>
#include "Repartition/client/Client.hpp"
#include "GroupMetadata.hpp"


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

        friend void Visitor<Reader<DataStream>>::readFrom<Group>(const Group& ev);
        friend void Visitor<Reader<JSONObject>>::readFrom<Group>(const Group& ev);
        friend void Visitor<Writer<DataStream>>::writeTo<Group>(Group& ev);
        friend void Visitor<Writer<JSONObject>>::writeTo<Group>(Group& ev);

    public:
        Group(QString name, id_type<Group> id, QObject* parent);

        template<typename Deserializer>
        Group(Deserializer&& vis, QObject* parent) :
            IdentifiedObject<Group> {vis, parent}
        {
            vis.writeTo(*this);
        }


        QString name() const;
        void setName(QString arg);

        void addClient(id_type<Client> clt);
        void removeClient(id_type<Client> clt);
        const QVector<id_type<Client>>& clients() const
        { return m_executingClients; }

    signals:
        void nameChanged(QString arg);

        void clientAdded(id_type<Client>);
        void clientRemoved(id_type<Client>);

    private:
        QString m_name;

        QVector<id_type<Client>> m_executingClients;
};
