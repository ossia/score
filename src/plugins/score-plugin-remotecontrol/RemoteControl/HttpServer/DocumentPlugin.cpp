#include "DocumentPlugin.hpp"
#include <RemoteControl/Settings/Model.hpp>

#include <score/tools/Bind.hpp>

namespace RemoteControl::HttpServer
{
DocumentPlugin::DocumentPlugin(const score::DocumentContext& doc, QObject* parent)
    : score::DocumentPlugin{doc, "RemoteControl::HttpServer::DocumentPlugin", parent}
{
  auto& model{m_context.app.settings<Settings::Model>()};

  m_server.set_path(model.getWebUiPath().toStdString());
  m_server.set_address(model.getServerAddress().toStdString());
  m_server.set_port(model.getServerPort());

  if (model.getEnabled() && model.getServerEnabled())
    m_server.start_thread();

  con(model
      , &Settings::Model::EnabledChanged
      , this
      , [this, &model] (bool e)
  {
    e && model.getServerEnabled()
    ? m_server.start_thread()
    : m_server.stop_thread();
  }, Qt::QueuedConnection);

  con(model
      , &Settings::Model::WebUiPathChanged
      , this
      , [this] (const QString& s)
  { m_server.set_path(s.toStdString()); }
      , Qt::QueuedConnection);

  con(model
      , &Settings::Model::ServerAddressChanged
      , this
      , [this] (const QString& s)
  { m_server.set_address(s.toStdString().c_str()); }
      , Qt::QueuedConnection);

  con(model
      , &Settings::Model::ServerPortChanged
      , this
      , [this] (unsigned short s)
  { m_server.set_port(s); }
      , Qt::QueuedConnection);

  con(model
      , &Settings::Model::ServerEnabledChanged
      , this
      , [this, &model] (bool e)
  {
    e && model.getEnabled()
    ? m_server.start_thread()
    : m_server.stop_thread();
  }, Qt::QueuedConnection);
}

DocumentPlugin::~DocumentPlugin() { }

} // namespace RemoteControl::HttpServer