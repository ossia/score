#include <Transport/DocumentPlugin.hpp>
#include <Transport/ApplicationPlugin.hpp>

#include <wobjectimpl.h>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

W_OBJECT_IMPL(Transport::DocumentPlugin)
namespace Transport
{

DocumentPlugin::DocumentPlugin(const score::DocumentContext& ctx, QObject* parent)
    : score::DocumentPlugin{ctx, "Transport", parent}
{

}

DocumentPlugin::~DocumentPlugin()
{

}

ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& app)
    : GUIApplicationPlugin{app}
{
}

void ApplicationPlugin::on_createdDocument(score::Document& doc)
{
  doc.model().addPluginModel(new DocumentPlugin{
      doc.context(), &doc.model()});
}
}
