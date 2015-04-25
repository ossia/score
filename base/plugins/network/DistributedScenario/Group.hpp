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
    public:
        Group(QString name, id_type<Group> id, QObject* parent);

        QString name() const;
        void setName(QString arg);

        void addClient(id_type<Client> clt);
        void removeClient(id_type<Client> clt);
        const auto& clients() const { return m_executingClients; }

    signals:
        void nameChanged(QString arg);

        void clientAdded(id_type<Client>);
        void clientRemoved(id_type<Client>);

    private:
        QString m_name;
        id_type<Group> m_id {};

        QVector<id_type<Client>> m_executingClients;
};
