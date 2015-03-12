#include "MessageMapper.hpp"
void MessageMapper::addHandler(QString addr, std::function<void (QByteArray)> fun)
{
    m_handlers[addr] = fun;
}


void MessageMapper::map(QString addr, QByteArray data)
{
    m_handlers[addr](data);
}


QList<QString> MessageMapper::addresses() const
{
    return m_handlers.keys();
}
