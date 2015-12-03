#include <QDebug>
#include <iscore/tools/Todo.hpp>
#include "MessageMapper.hpp"
#include "Serialization/NetworkMessage.hpp"

void MessageMapper::addHandler(QString addr, std::function<void(NetworkMessage)> fun)
{
    ISCORE_ASSERT(!m_handlers.contains(addr));
    m_handlers[addr] = fun;
}


void MessageMapper::map(NetworkMessage m)
{
    auto it = m_handlers.find(m.address);
    if(it != m_handlers.end())
        (*it)(m);
    else
        qDebug() << "Address" << m.address << "not handled.";
}


QList<QString> MessageMapper::addresses() const
{
    return m_handlers.keys();
}
