// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <core/document/Document.hpp>

#include <algorithm>
#include <vector>

#include "DeviceExplorerApplicationPlugin.hpp"
#include <core/document/DocumentModel.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/tools/IdentifierGeneration.hpp>

struct VisitorVariant;

namespace Explorer
{
ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& app)
    : GUIApplicationPlugin{app}
{
}

void ApplicationPlugin::on_newDocument(score::Document& doc)
{
  doc.model().addPluginModel(new DeviceDocumentPlugin{
      doc.context(), getStrongId(doc.model().pluginModels()), &doc.model()});
}

void ApplicationPlugin::on_documentChanged(
    score::Document* olddoc, score::Document* newdoc)
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
