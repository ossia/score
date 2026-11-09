#pragma once
#include <score/plugins/Interface.hpp>
#include <score/plugins/InterfaceList.hpp>

#include <score_lib_device_export.h>

#include <memory>
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

  /**
   * @brief Listens to every address of every device of the document.
   *
   * The model only gets the values of the addresses listened to, which are
   * otherwise only those the explorer shows. Listening lasts as long as the
   * returned object; destroying it puts each device back to listening to what
   * it listened to before. The values the addresses hold already are not
   * reported, only those that arrive afterwards.
   */
  virtual std::shared_ptr<void>
  listenToAllAddresses(const score::DocumentContext& ctx) const noexcept
  {
    return {};
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
