#include "GroupHeaderItem.hpp"
#include "DistributedScenario/Group.hpp"

GroupHeaderItem::GroupHeaderItem(const Group& grp):
    QTableWidgetItem{grp.name()},
    group{grp.id()}
{

}
