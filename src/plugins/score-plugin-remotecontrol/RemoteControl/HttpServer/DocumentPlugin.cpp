#include "DocumentPlugin.hpp"
#include <RemoteControl/Settings/Model.hpp>

#include <score/tools/Bind.hpp>

namespace RemoteControl::HttpServer
{
DocumentPlugin::DocumentPlugin(const score::DocumentContext& doc, QObject* parent)
    : score::DocumentPlugin{doc, "RemoteControl::HttpServer::DocumentPlugin", parent}
{
  auto& model{m_context.app.settings<Settings::Model>()};

  if (model.getServerEnabled())
    m_server.start_thread();

  con(model
      , &Settings::Model::ServerEnabledChanged
      , this
      , [this] (bool e)
  { e ? m_server.start_thread() : m_server.stop_thread(); }
      , Qt::QueuedConnection);
}

DocumentPlugin::~DocumentPlugin() { }

} // namespace RemoteControl::HttpServer