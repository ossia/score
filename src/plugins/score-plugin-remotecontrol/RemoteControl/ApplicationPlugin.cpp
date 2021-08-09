#include <score/tools/IdentifierGeneration.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <RemoteControl/ApplicationPlugin.hpp>
#include <RemoteControl/DocumentPlugin.hpp>

#include <RemoteControl/Http_server.hpp>

namespace RemoteControl
{
ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& app)
    : GUIApplicationPlugin{app}
{
}

void ApplicationPlugin::on_createdDocument(score::Document& doc)
{
  doc.model().addPluginModel(new DocumentPlugin{
      doc.context(), &doc.model()});

  Http_server Http_server;

  Http_server.open_server();
}

}
