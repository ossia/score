#include "SessionHeaderItem.hpp"

#include <client/Client.hpp>

SessionHeaderItem::SessionHeaderItem(const Client& client):
    QTableWidgetItem{client.name()},
    client{client.id()}
{

}
