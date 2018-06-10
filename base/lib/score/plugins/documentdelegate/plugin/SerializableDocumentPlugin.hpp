#pragma once
#include <score/plugins/documentdelegate/plugin/DocumentPluginBase.hpp>
#include <wobjectdefs.h>

namespace score
{
/**
 * @brief Document plug-in with serializable data.
 *
 * A difference with other class is that this class has two points
 * at which it can save and reload data :
 *
 * * The pre-document point : the object information, etc.
 *   Saved and loaded **before** the DocumentModel.
 *   Uses the default mechanism.
 *
 * * The post-document point.
 *   If there are informations that need to be reloaded **after**
 *   the DocumentModel was loaded. For instance components.
 *   This happens after the object has been constructed.
 */
class SCORE_LIB_BASE_EXPORT SerializableDocumentPlugin
    : public DocumentPlugin
    , public SerializableInterface<DocumentPluginFactory>
{
  W_OBJECT(SerializableDocumentPlugin)
public:
  virtual void serializeAfterDocument(const VisitorVariant& vis) const;
  virtual void reloadAfterDocument(const VisitorVariant& vis);

protected:
  using DocumentPlugin::DocumentPlugin;
  using ConcreteKey = UuidKey<DocumentPluginFactory>;

  virtual ~SerializableDocumentPlugin();
};

/**
 * @brief Reimplement to instantiate document plug-ins.
 */
class SCORE_LIB_BASE_EXPORT DocumentPluginFactory
    : public score::InterfaceBase
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
  object_type* loadMissing(
      const VisitorVariant& vis,
      score::DocumentContext& doc,
      QObject* parent) const;
};

template <typename T>
class DocumentPluginFactory_T final : public score::DocumentPluginFactory
{
public:
  T* load(
      const VisitorVariant& var,
      score::DocumentContext& doc,
      QObject* parent) override
  {
    return deserialize_dyn(var, [&](auto&& deserializer) {
      return new T{doc, deserializer, parent};
    });
  }

  static UuidKey<score::DocumentPluginFactory> static_concreteKey()
  {
    return Metadata<ConcreteKey_k, T>::get();
  }

  UuidKey<score::DocumentPluginFactory> concreteKey() const
      noexcept final override
  {
    return Metadata<ConcreteKey_k, T>::get();
  }
};
}
