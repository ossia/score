#pragma once
#include <boost/optional/optional.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <QTableWidget>

class Group;

class GroupHeaderItem : public QTableWidgetItem
{
    public:
        explicit GroupHeaderItem(const Group& group);

        const Id<Group> group;
};
