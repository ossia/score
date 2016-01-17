#include "SessionHeaderItem.hpp"

#include <client/Client.hpp>

namespace Network
{
SessionHeaderItem::SessionHeaderItem(const Client& clt):
    QTableWidgetItem{clt.name()},
    client{clt.id()}
{

}
}
