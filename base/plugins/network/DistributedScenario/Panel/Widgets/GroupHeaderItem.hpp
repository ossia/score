#pragma once
#include <QTableWidgetItem>
#include <iscore/tools/SettableIdentifier.hpp>

class Group;

class GroupHeaderItem : public QTableWidgetItem
{
    public:
        GroupHeaderItem(const Group& group);

        const id_type<Group> group;
};
