#pragma once
#include <Device/Protocol/ProtocolFactoryInterface.hpp>

#include <score/plugins/customfactory/FactoryFamily.hpp>

namespace Device
{
class SCORE_LIB_DEVICE_EXPORT ProtocolFactoryList final
    : public score::InterfaceList<ProtocolFactory>
{
};
}
