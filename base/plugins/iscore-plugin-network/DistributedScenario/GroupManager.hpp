#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <vector>

#include <iscore/tools/SettableIdentifier.hpp>

class Group;
class QObject;

class GroupManager : public IdentifiedObject<GroupManager>
{
        Q_OBJECT
    public:
        explicit GroupManager(QObject* parent);

        template<typename Deserializer>
        GroupManager(Deserializer&& vis, QObject* parent) :
            IdentifiedObject{vis, parent}
        {
            vis.writeTo(*this);
        }

        void addGroup(Group* group);
        void removeGroup(Id<Group> group);

        const std::vector<Group*>& groups() const;
        Group* group(const Id<Group>& id) const;
        Id<Group> defaultGroup() const;

    signals:
        void groupAdded(Id<Group>);
        void groupRemoved(Id<Group>);

    private:
        std::vector<Group*> m_groups;
};
