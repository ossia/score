#include "GfxApplicationPlugin.hpp"

#include <Execution/DocumentPlugin.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

namespace Gfx
{

DocumentPlugin::DocumentPlugin(
    const score::DocumentContext& ctx,
    QObject* parent)
    : score::DocumentPlugin{ctx, "Gfx::DocumentPlugin", parent}
    , context{ctx}
{
  auto& exec_plug = ctx.plugin<Execution::DocumentPlugin>();
  exec_plug.registerAction(exec);
}

DocumentPlugin::~DocumentPlugin() { }

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
