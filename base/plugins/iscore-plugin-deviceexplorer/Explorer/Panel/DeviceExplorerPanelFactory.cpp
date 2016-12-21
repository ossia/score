#include "DeviceExplorerPanelFactory.hpp"
#include "DeviceExplorerPanelDelegate.hpp"

namespace Explorer
{

std::unique_ptr<iscore::PanelDelegate>
PanelDelegateFactory::make(const iscore::GUIApplicationContext& ctx)
{
  return std::make_unique<PanelDelegate>(ctx);
}
}
