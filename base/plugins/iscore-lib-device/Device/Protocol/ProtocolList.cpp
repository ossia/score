#include "ProtocolList.hpp"

iscore::FactoryBaseKey DynamicProtocolList::name() const
{
    return ProtocolFactory::staticFactoryKey();
}

void DynamicProtocolList::insert(iscore::FactoryInterfaceBase* e)
{
    if(auto pf = dynamic_cast<ProtocolFactory*>(e))
        m_list.inscribe(pf);
}
