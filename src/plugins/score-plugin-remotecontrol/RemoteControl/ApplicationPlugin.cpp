
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/UuidKeySerialization.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <RemoteControl/ApplicationPlugin.hpp>

#include <RemoteControl/Controller/DocumentPlugin.hpp>
#include <RemoteControl/Websockets/DocumentPlugin.hpp>
#include <RemoteControl/HttpServer/DocumentPlugin.hpp>


namespace RemoteControl
{
ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& app)
    : GUIApplicationPlugin{app}
{
}

void ApplicationPlugin::on_createdDocument(score::Document& doc)
{
  doc.model().addPluginModel(new WS::DocumentPlugin{doc.context(), &doc.model()});
  doc.model().addPluginModel(
      new Controller::DocumentPlugin{doc.context(), &doc.model()});
  doc.model().addPluginModel(
      new HttpServer::DocumentPlugin{doc.context(), &doc.model()});
}

}
