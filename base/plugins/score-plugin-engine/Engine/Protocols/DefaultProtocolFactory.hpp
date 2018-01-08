#pragma once
#include <Device/Protocol/ProtocolFactoryInterface.hpp>
#include <Explorer/Explorer/Widgets/AddressEditDialog.hpp>

namespace Engine
{
namespace Network
{
class DefaultProtocolFactory : public Device::ProtocolFactory
{
public:
  using Device::ProtocolFactory::ProtocolFactory;

  Device::AddAddressDialog* makeAddAddressDialog(const Device::DeviceSettings& dev, const score::DocumentContext& ctx, QWidget* parent) override
  {
    return new Explorer::AddressEditDialog{parent};
  }
};
}
}
