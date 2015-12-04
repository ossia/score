#pragma once
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

class ISCORE_LIB_DEVICE_EXPORT DynamicProtocolList : public iscore::FactoryListInterface
{
        ISCORE_FACTORY_LIST_DECL(ProtocolFactory)
};
