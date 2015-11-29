#include <algorithm>
#include <iterator>

#include "Group.hpp"
#include <iscore/tools/IdentifiedObject.hpp>

class Client;
class QObject;


Group::Group(QString name, Id<Group> id, QObject* parent):
    IdentifiedObject<Group>{id, "Group", parent},
    m_name{name}
{

}

QString Group::name() const
{
    return m_name;
}

void Group::setName(QString arg)
{
    if (m_name == arg)
        return;

    m_name = arg;
    emit nameChanged(arg);
}

void Group::addClient(Id<Client> clt)
{
    m_executingClients.push_back(clt);
    emit clientAdded(clt);
}

void Group::removeClient(Id<Client> clt)
{
    auto it = std::find(std::begin(m_executingClients), std::end(m_executingClients), clt);
    ISCORE_ASSERT(it != std::end(m_executingClients));

    m_executingClients.erase(it);
    emit clientRemoved(clt);
}
