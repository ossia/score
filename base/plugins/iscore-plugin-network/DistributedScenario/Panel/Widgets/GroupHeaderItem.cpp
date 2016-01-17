#include "GroupHeaderItem.hpp"
#include "DistributedScenario/Group.hpp"


namespace Network
{
GroupHeaderItem::GroupHeaderItem(const Group& grp):
    QTableWidgetItem{grp.name()},
    group{grp.id()}
{

}
}
