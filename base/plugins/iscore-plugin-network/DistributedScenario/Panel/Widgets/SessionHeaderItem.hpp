#pragma once
#include <QTableWidgetItem>
#include <iscore/tools/SettableIdentifier.hpp>
class Client;

class SessionHeaderItem : public QTableWidgetItem
{
    public:
        explicit SessionHeaderItem(const Client& client);

        const id_type<Client> client;
};
