#pragma once
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

namespace score
{

template <typename DocPlugin>
auto& addDocumentPlugin(score::Document& doc)
{
  auto& model = doc.model();
  auto plug = new DocPlugin{doc.context(), getStrongId(model.pluginModels()),
                            &model};
  model.addPluginModel(plug);
  return *plug;
}
}
