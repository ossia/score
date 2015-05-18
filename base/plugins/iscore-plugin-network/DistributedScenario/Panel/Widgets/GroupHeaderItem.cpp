#include "GroupHeaderItem.hpp"
#include "DistributedScenario/Group.hpp"

GroupHeaderItem::GroupHeaderItem(const Group& group):
    QTableWidgetItem{group.name()},
    group{group.id()}
{

}
