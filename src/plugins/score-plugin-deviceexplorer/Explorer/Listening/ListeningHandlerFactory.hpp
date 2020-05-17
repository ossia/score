#pragma once
#include <Explorer/Listening/ListeningHandler.hpp>

#include <score/plugins/Interface.hpp>

#include <score_plugin_deviceexplorer_export.h>
namespace score
{
struct DocumentContext;
}
namespace Explorer
{
class DeviceDocumentPlugin;
class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT ListeningHandlerFactory : public score::InterfaceBase
{
  SCORE_INTERFACE(ListeningHandlerFactory, "42828393-b8de-45a6-b79f-811eea2e1a40")

public:
  virtual ~ListeningHandlerFactory();

  virtual std::unique_ptr<Explorer::ListeningHandler>
  make(const DeviceDocumentPlugin& plug, const score::DocumentContext& ctx) = 0;
};
}
