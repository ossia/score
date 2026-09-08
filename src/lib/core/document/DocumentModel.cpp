// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include <score/model/IdentifiedObject.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateModel.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>

#include <core/document/DocumentModel.hpp>
class QObject;
#include <score/model/Identifier.hpp>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::DocumentModel)
namespace score
{
DocumentModel::DocumentModel(
    const Id<DocumentModel>& id, const score::DocumentContext& ctx,
    DocumentDelegateFactory& fact, QObject* parent)
    : IdentifiedObject{id, "DocumentModel", parent}
{
  fact.make(ctx, m_model, this);
  for(auto& appPlug : ctx.app.guiApplicationPlugins())
  {
    appPlug->on_initDocument(ctx.document);
  }
}

DocumentModel::~DocumentModel()
{
  // We remove the plug-ins first, in reverse order of creation
  // (to maintain consistency with eg. execution plugin having to disappear before explorer plugin)
  //
  // The pointer leaves the vector *before* it is deleted. A plug-in destructor
  // can reach DocumentContext::findPlugin() / plugin<T>(), both of which
  // dynamic_cast every entry of this very vector; an entry that has already
  // been deleted but is still listed makes that a read through a destroyed
  // vtable. The window is one destructor wide and the freed page is usually
  // still mapped and still holds the old pointer, so the fault only shows once
  // the allocator reuses the block.
  while(!m_pluginModels.empty())
  {
    auto* plug = m_pluginModels.back();
    m_pluginModels.pop_back();
    delete plug;
  }
  delete m_model;
}

void DocumentModel::on_documentClosing()
{
  m_model->close();
}

void DocumentModel::addPluginModel(DocumentPlugin* m)
{
  m->setParent(this);
  m_pluginModels.push_back(m);
  pluginModelsChanged();
}
}
