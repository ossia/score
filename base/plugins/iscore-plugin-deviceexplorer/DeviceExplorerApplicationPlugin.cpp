#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <core/document/Document.hpp>

#include <algorithm>
#include <vector>

#include "DeviceExplorerApplicationPlugin.hpp"
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <core/document/DocumentModel.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <iscore/tools/IdentifierGeneration.hpp>

struct VisitorVariant;

namespace Explorer
{
ApplicationPlugin::ApplicationPlugin(const iscore::GUIApplicationContext& app)
    : GUIApplicationContextPlugin{app}
{
}

void ApplicationPlugin::on_newDocument(iscore::Document& doc)
{
  doc.model().addPluginModel(new DeviceDocumentPlugin{
      doc.context(), getStrongId(doc.model().pluginModels()), &doc.model()});
}

void ApplicationPlugin::on_documentChanged(
    iscore::Document* olddoc, iscore::Document* newdoc)
{
  if (olddoc)
  {
    auto& doc_plugin = olddoc->context().plugin<DeviceDocumentPlugin>();
    doc_plugin.setConnection(false);
  }

  if (newdoc)
  {
    auto& doc_plugin = newdoc->context().plugin<DeviceDocumentPlugin>();
    doc_plugin.setConnection(true);
  }
}
}
