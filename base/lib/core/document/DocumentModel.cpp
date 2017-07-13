// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <core/document/DocumentModel.hpp>
#include <iscore/plugins/application/GUIApplicationPlugin.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateModel.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/model/IdentifiedObject.hpp>
class QObject;
#include <iscore/model/Identifier.hpp>

namespace iscore
{
DocumentModel::DocumentModel(
    const Id<DocumentModel>& id,
    const iscore::DocumentContext& ctx,
    DocumentDelegateFactory& fact,
    QObject* parent)
    : IdentifiedObject{id, "DocumentModel", parent}
    , m_model{fact.make(ctx, this)}
{
  for (auto& appPlug : ctx.app.guiApplicationPlugins())
  {
    appPlug->on_initDocument(ctx.document);
  }
}

DocumentModel::~DocumentModel()
{
  auto p = m_pluginModels;

  // We remove the plug-ins first.
  for (auto plug : p)
  {
    delete plug;
    m_pluginModels.erase(m_pluginModels.begin());
  }
  delete m_model;
}

void DocumentModel::addPluginModel(DocumentPlugin* m)
{
  m->setParent(this);
  m_pluginModels.push_back(m);
  emit pluginModelsChanged();
}

}
