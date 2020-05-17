#pragma once
#include <score/plugins/InterfaceList.hpp>
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
  auto plug = new DocPlugin{doc.context(), getStrongId(model.pluginModels()), &model};
  model.addPluginModel(plug);
  return *plug;
}

/**
 * @brief Reimplement to instantiate document plug-ins.
 */
class SCORE_LIB_BASE_EXPORT DocumentPluginFactory : public score::InterfaceBase
{
  SCORE_INTERFACE(DocumentPluginFactory, "570faa0b-f100-4039-a2f0-b60347c4e581")
public:
  virtual ~DocumentPluginFactory();

  virtual DocumentPlugin*
  load(const VisitorVariant& var, score::DocumentContext& doc, QObject* parent)
      = 0;
};
class SCORE_LIB_BASE_EXPORT DocumentPluginFactoryList final
    : public score::InterfaceList<score::DocumentPluginFactory>
{
public:
  using object_type = DocumentPlugin;
  ~DocumentPluginFactoryList();
  object_type*
  loadMissing(const VisitorVariant& vis, score::DocumentContext& doc, QObject* parent) const;
};

template <typename T>
class DocumentPluginFactory_T final : public score::DocumentPluginFactory
{
public:
  T* load(const VisitorVariant& var, score::DocumentContext& doc, QObject* parent) override
  {
    return deserialize_dyn(var, [&](auto&& deserializer) {
      return new T{doc, deserializer, parent};
    });
  }

  static UuidKey<score::DocumentPluginFactory> static_concreteKey()
  {
    return Metadata<ConcreteKey_k, T>::get();
  }

  UuidKey<score::DocumentPluginFactory> concreteKey() const noexcept final override
  {
    return Metadata<ConcreteKey_k, T>::get();
  }
};

}
