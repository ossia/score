#pragma once
#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>

#include <score_lib_device_export.h>
namespace score
{
struct DocumentContext;
}
namespace Device
{
class DeviceInterface;
class NodeBasedItemModel;

class SCORE_LIB_DEVICE_EXPORT DeviceModelProvider : public score::InterfaceBase
{
  SCORE_INTERFACE(DeviceModelProvider, "2f641f1d-46f8-4c88-9038-7e84957e4a03")

public:
  ~DeviceModelProvider() override;
  virtual Device::NodeBasedItemModel*
  getNodeModel(const score::DocumentContext& ctx) const noexcept = 0;

  //! Finds a device of the document by name, even if the explorer does not show it
  virtual Device::DeviceInterface*
  findDevice(const QString& name, const score::DocumentContext& ctx) const noexcept
  {
    return nullptr;
  }
};

class SCORE_LIB_DEVICE_EXPORT DeviceModelProviderList final
    : public score::InterfaceList<DeviceModelProvider>
{
public:
  ~DeviceModelProviderList() override;
  DeviceModelProvider* getBestProvider(const score::DocumentContext& ctx) const noexcept;
};
}
