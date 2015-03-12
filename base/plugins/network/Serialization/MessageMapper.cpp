#include "MessageMapper.hpp"
#include <QDebug>
void MessageMapper::addHandler(QString addr, std::function<void (QByteArray)> fun)
{
    m_handlers[addr] = fun;
}


void MessageMapper::map(QString addr, QByteArray data)
{
    if(m_handlers.contains(addr))
        m_handlers[addr](data);
    else
        qDebug() << "Address" << addr << "not handled.";
}


QList<QString> MessageMapper::addresses() const
{
    return m_handlers.keys();
}
