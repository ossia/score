#include "ProtocolList.hpp"
#include "ProtocolFactoryInterface.hpp"

QStringList ProtocolList::protocols() const
{
    QStringList lst;

    for(auto& elt : m_protocols)
    {
        lst.append(elt->name());
    }

    return lst;
}

ProtocolFactory* ProtocolList::protocol(const QString& name) const
{
    auto it = std::find_if(m_protocols.begin(),
                           m_protocols.end(),
                           [&name](ProtocolFactory * p)
    {
        return p->name() == name;
    });

    return it != m_protocols.end() ? *it : nullptr;
}

void ProtocolList::registerFactory(iscore::FactoryInterface* arg)
{
    auto p = safe_cast<ProtocolFactory*>(arg);
    auto it = std::find_if(m_protocols.begin(),
                           m_protocols.end(),
                           [&p](ProtocolFactory * inner_p)
    {
        return inner_p->name() == p->name();
    });

    if(it == m_protocols.end())
    {
        m_protocols.push_back(p);
    }
    else
    {
        qDebug() << "Alert : a Protocol with the name" << p->name() << "already exists.";
    }
}

