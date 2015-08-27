#pragma once
#include <QWidget>
#include <iscore/tools/SettableIdentifier.hpp>
class Group;
class GroupWidget : public QWidget
{
    public:
        GroupWidget(Group* group, QWidget* parent);

        Id<Group> id() const;

    private:
        Group* m_group;
};
