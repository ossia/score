#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <vector>

class Group;
class GroupManager : public IdentifiedObject<GroupManager>
{
        Q_OBJECT
    public:
        GroupManager(QObject* parent);

        void addGroup(Group* group);
        void removeGroup(id_type<Group> group);

        const std::vector<Group*>& groups() const;
        Group* group(const id_type<Group>& id) const;
        id_type<Group> defaultGroup() const;

    signals:
        void groupAdded(id_type<Group>);
        void groupRemoved(id_type<Group>);

    private:
        std::vector<Group*> m_groups;
};
