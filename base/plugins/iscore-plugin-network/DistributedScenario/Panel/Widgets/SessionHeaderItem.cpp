#include "SessionHeaderItem.hpp"

#include <client/Client.hpp>

SessionHeaderItem::SessionHeaderItem(const Client& clt):
    QTableWidgetItem{clt.name()},
    client{clt.id()}
{

}
