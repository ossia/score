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
  for (auto& appPlug : ctx.app.applicationPlugins())
  {
    appPlug->on_initDocument(ctx.document);
  }
}

DocumentModel::~DocumentModel()
{
  // We remove the plug-ins first.
  for (auto plug : m_pluginModels)
  {
    delete plug;
  }
  delete m_model;
}

void DocumentModel::addPluginModel(DocumentPlugin* m)
{
  m->setParent(this);
  m_pluginModels.push_back(m);
  emit pluginModelsChanged();
}

void DocumentModel::setNewSelection(const Selection& s)
{
  m_model->setNewSelection(s);
}
}
