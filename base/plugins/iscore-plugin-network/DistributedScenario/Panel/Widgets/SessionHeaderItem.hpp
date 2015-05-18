#pragma once
#include <QTableWidgetItem>
#include <iscore/tools/SettableIdentifier.hpp>
class Client;

class SessionHeaderItem : public QTableWidgetItem
{
    public:
        SessionHeaderItem(const Client& client);

        const id_type<Client> client;
};
