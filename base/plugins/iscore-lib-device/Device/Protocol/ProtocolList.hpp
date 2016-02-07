#pragma once
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <iscore/plugins/customfactory/FactoryMap.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

namespace Device
{
class ISCORE_LIB_DEVICE_EXPORT DynamicProtocolList :
        public iscore::ConcreteFactoryList<ProtocolFactory>
{
};
}
