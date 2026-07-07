#pragma once
#include "Http_server.hpp"

#include <score_plugin_remotecontrol_export.h>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>

namespace RemoteControl::HttpServer
{
struct SCORE_PLUGIN_REMOTECONTROL_EXPORT DocumentPlugin : score::DocumentPlugin
{
public:
  DocumentPlugin(const score::DocumentContext& doc, QObject* parent);
  ~DocumentPlugin();

private:
  Http_server m_server;
};

} // namespace RemoteControl::HttpServer