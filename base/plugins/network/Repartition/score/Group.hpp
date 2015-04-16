#pragma once
#include <QString>
#include <iscore/tools/IdentifiedObject.hpp>

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
            IdentifiedObject<Group>{id, "Group", parent}
        {

        }

        QString name() const
        {
            return m_name;
        }

    public slots:
        void setName(QString arg)
        {
            if (m_name == arg)
                return;

            m_name = arg;
            emit nameChanged(arg);
        }

    signals:
        void nameChanged(QString arg);

    private:
        QString m_name;
};
