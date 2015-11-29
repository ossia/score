#include <QDebug>

#include "MessageMapper.hpp"
#include "Serialization/NetworkMessage.hpp"

void MessageMapper::addHandler(QString addr, std::function<void(NetworkMessage)> fun)
{
    m_handlers[addr] = fun;
}


void MessageMapper::map(NetworkMessage m)
{
    if(m_handlers.contains(m.address))
        m_handlers[m.address](m);
    else
        qDebug() << "Address" << m.address << "not handled.";
}


QList<QString> MessageMapper::addresses() const
{
    return m_handlers.keys();
}
