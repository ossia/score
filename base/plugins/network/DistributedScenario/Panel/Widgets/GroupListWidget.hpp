#pragma once
#include <QWidget>
#include <iscore/tools/SettableIdentifier.hpp>
class Group;
class GroupWidget;
class GroupManager;

class GroupListWidget : public QWidget
{
    public:
        GroupListWidget(const GroupManager* mgr, QWidget* parent);

    private:
        void addGroup(const id_type<Group>& id);
        void removeGroup(const id_type<Group>& id);

        const GroupManager* m_mgr{};
        QList<GroupWidget*> m_widgets;
};
