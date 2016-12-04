#pragma once
#include <iscore/document/DocumentContext.hpp>

#include <QString>
#include <iscore/plugins/customfactory/SerializableInterface.hpp>
#include <iscore/serialization/VisitorCommon.hpp>
#include <vector>

class QWidget;
namespace iscore
{
class Document;
}

// TODO DocumentPlugin -> system
namespace iscore
{
/**
 * @brief Extend a document with custom data and systems.
 */
class ISCORE_LIB_BASE_EXPORT DocumentPlugin
    : public IdentifiedObject<DocumentPlugin>
{
  Q_OBJECT
public:
  DocumentPlugin(
      const iscore::DocumentContext&,
      Id<DocumentPlugin>
          id,
      const QString& name,
      QObject* parent);

  virtual ~DocumentPlugin();

  const iscore::DocumentContext& context() const
  {
    return m_context;
  }

  template <typename Impl>
  explicit DocumentPlugin(
      const iscore::DocumentContext& ctx,
      Deserializer<Impl>& vis,
      QObject* parent)
      : IdentifiedObject{vis, parent}, m_context{ctx}
  {
  }

protected:
  const iscore::DocumentContext& m_context;
};

class DocumentPluginFactory;

/**
 * @brief Document plug-in with serializable data.
 */
class ISCORE_LIB_BASE_EXPORT SerializableDocumentPlugin
    : public DocumentPlugin,
      public SerializableInterface<DocumentPluginFactory>
{
protected:
  using DocumentPlugin::DocumentPlugin;
  using ConcreteFactoryKey = UuidKey<DocumentPluginFactory>;

  virtual ~SerializableDocumentPlugin();
};

/**
 * @brief Reimplement to instantiate document plug-ins.
 */
class ISCORE_LIB_BASE_EXPORT DocumentPluginFactory
    : public iscore::AbstractFactory<DocumentPluginFactory>
{
  ISCORE_ABSTRACT_FACTORY("570faa0b-f100-4039-a2f0-b60347c4e581")
public:
  virtual ~DocumentPluginFactory();

  virtual DocumentPlugin* load(
      const VisitorVariant& var, iscore::DocumentContext& doc, QObject* parent)
      = 0;
};
class ISCORE_LIB_BASE_EXPORT DocumentPluginFactoryList final
    : public iscore::ConcreteFactoryList<iscore::DocumentPluginFactory>
{
public:
  using object_type = DocumentPlugin;
  object_type* loadMissing(
      const VisitorVariant& vis,
      iscore::DocumentContext& doc,
      QObject* parent) const;
};

template <typename T>
class DocumentPluginFactory_T final : public iscore::DocumentPluginFactory
{
public:
  T* load(
      const VisitorVariant& var,
      iscore::DocumentContext& doc,
      QObject* parent) override
  {
    return deserialize_dyn(var, [&](auto&& deserializer) {
      return new T{doc, deserializer, parent};
    });
  }

  static UuidKey<iscore::DocumentPluginFactory> static_concreteFactoryKey()
  {
    return Metadata<ConcreteFactoryKey_k, T>::get();
  }

  UuidKey<iscore::DocumentPluginFactory>
  concreteFactoryKey() const final override
  {
    return Metadata<ConcreteFactoryKey_k, T>::get();
  }
};
}
