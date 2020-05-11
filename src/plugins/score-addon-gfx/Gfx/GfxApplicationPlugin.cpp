#include "GfxApplicationPlugin.hpp"

#include <score/tools/IdentifierGeneration.hpp>

#include <core/application/ApplicationSettings.hpp>
#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <Execution/DocumentPlugin.hpp>

namespace Gfx
{
DocumentPlugin::DocumentPlugin(
    const score::DocumentContext& ctx,
    Id<score::DocumentPlugin> id,
    QObject* parent)
    : score::DocumentPlugin{ctx, std::move(id), "Gfx::DocumentPlugin", parent}
{
  auto& exec_plug = ctx.plugin<Execution::DocumentPlugin>();
  exec_plug.registerAction(exec);
}

DocumentPlugin::~DocumentPlugin() {}

ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& app)
    : GUIApplicationPlugin{app}
{
}

void ApplicationPlugin::on_createdDocument(score::Document& doc)
{
  doc.model().addPluginModel(new DocumentPlugin{
      doc.context(), getStrongId(doc.model().pluginModels()), &doc.model()});
}

}
