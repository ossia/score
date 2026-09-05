#include "NetworkSessionInterface.hpp"

#include <score/application/GUIApplicationContext.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>

namespace score
{
NetworkSessionInterface::~NetworkSessionInterface() = default;

NetworkSessionInterface* findNetworkSessionInterface(const GUIApplicationContext& ctx)
{
  for(auto* plug : ctx.guiApplicationPlugins())
    if(auto iface = dynamic_cast<NetworkSessionInterface*>(plug))
      return iface;
  return nullptr;
}
}
