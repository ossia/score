#pragma once
#include <Explorer/Listening/ListeningHandlerFactory.hpp>

#include <score/plugins/InterfaceList.hpp>

#include <score_plugin_deviceexplorer_export.h>
namespace Explorer
{
class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT ListeningHandlerFactoryList final
    : public score::InterfaceList<Explorer::ListeningHandlerFactory>
{
public:
  virtual ~ListeningHandlerFactoryList();

  std::unique_ptr<Explorer::ListeningHandler>
  make(const Explorer::DeviceDocumentPlugin& plug, const score::DocumentContext& ctx) const;
};
}
