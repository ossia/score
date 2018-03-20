#pragma once
#include <Device/Protocol/DeviceInterface.hpp>
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

    Device::AddressDialog* makeEditAddressDialog(
        const Device::AddressSettings& set,
        const Device::DeviceInterface& dev,
        const score::DocumentContext& ctx,
        QWidget* parent) override
    {
      auto ptr = new Explorer::AddressEditDialog{set, parent};

      ptr->setCanRename(dev.capabilities().canRenameNode);
      ptr->setCanEditProperties(dev.capabilities().canSetProperties);

      return ptr;

    }
    Device::AddressDialog* makeAddAddressDialog(const Device::DeviceInterface& dev, const score::DocumentContext& ctx, QWidget* parent) override
    {
      auto ptr = new Explorer::AddressEditDialog{parent};

      ptr->setCanRename(dev.capabilities().canRenameNode);
      ptr->setCanEditProperties(dev.capabilities().canSetProperties);

      return ptr;
    }
};
}
}
