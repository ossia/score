#include <score/tools/IdentifierGeneration.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <RemoteControl/ApplicationPlugin.hpp>
#include <RemoteControl/DocumentPlugin.hpp>


namespace RemoteControl
{
ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& app)
    : GUIApplicationPlugin{app}
{
}

ApplicationPlugin::~ApplicationPlugin()
{
    std::remove((m_server.m_buildWasmPath + "remote.html").c_str());
    std::rename((m_server.m_buildWasmPath + "remote.html~").c_str(), (m_server.m_buildWasmPath + "remote.html").c_str());
}

void ApplicationPlugin::on_createdDocument(score::Document& doc)
{
  doc.model().addPluginModel(new DocumentPlugin{
      doc.context(), &doc.model()});

    m_server.start_thread();
}

}
