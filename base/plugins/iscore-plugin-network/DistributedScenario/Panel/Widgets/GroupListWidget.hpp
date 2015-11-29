#pragma once
#include <QList>
#include <QWidget>

class Group;
class GroupManager;
class GroupWidget;
template <typename tag, typename impl> class id_base_t;

class GroupListWidget : public QWidget
{
    public:
        GroupListWidget(const GroupManager* mgr, QWidget* parent);

    private:
        void addGroup(const Id<Group>& id);
        void removeGroup(const Id<Group>& id);

        const GroupManager* m_mgr{};
        QList<GroupWidget*> m_widgets;
};
