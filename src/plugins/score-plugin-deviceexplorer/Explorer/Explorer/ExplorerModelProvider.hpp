#pragma once
#include <Device/Widgets/DeviceModelProvider.hpp>

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>

#include <score/document/DocumentContext.hpp>

namespace Explorer
{
class ModelProvider final : public Device::DeviceModelProvider
{
  SCORE_CONCRETE("a06e7c4e-a817-411c-8412-1d3f4ddce5e7")
public:
  ~ModelProvider() override = default;

  Device::NodeBasedItemModel*
  getNodeModel(const score::DocumentContext& ctx) const noexcept override
  {
    auto plug = ctx.findPlugin<DeviceDocumentPlugin>();
    if(plug)
    {
      return &plug->explorer();
    }
    return nullptr;
  }
};
}
