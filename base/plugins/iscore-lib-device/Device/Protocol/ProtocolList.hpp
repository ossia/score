#pragma once
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
using ProtocolList = GenericFactoryMap_T<ProtocolFactory, ProtocolFactoryKey>;

class DynamicProtocolList : public iscore::FactoryListInterface
{
        ISCORE_FACTORY_LIST_DECL(ProtocolFactory)
};
