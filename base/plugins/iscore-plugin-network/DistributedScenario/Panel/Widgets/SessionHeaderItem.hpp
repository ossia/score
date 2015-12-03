#pragma once
#include <boost/optional/optional.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <QTableWidget>

class Client;

class SessionHeaderItem : public QTableWidgetItem
{
    public:
        explicit SessionHeaderItem(const Client& client);

        const Id<Client> client;
};
