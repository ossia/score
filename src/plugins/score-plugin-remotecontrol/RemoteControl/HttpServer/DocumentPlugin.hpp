#pragma once
#include "HttpServer.hpp"

#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>

namespace RemoteControl::HttpServer
{
struct DocumentPlugin : score::DocumentPlugin
{
public:
  DocumentPlugin(const score::DocumentContext& doc, QObject* parent);
  ~DocumentPlugin();

private:
  HttpServer m_server;
};

}