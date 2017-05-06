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
      Impl& vis,
      QObject* parent)
      : IdentifiedObject{vis, parent}, m_context{ctx}
  {
  }

  virtual void on_documentClosing();
protected:
  const iscore::DocumentContext& m_context;
};

class DocumentPluginFactory;

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
class ISCORE_LIB_BASE_EXPORT SerializableDocumentPlugin
    : public DocumentPlugin,
      public SerializableInterface<DocumentPluginFactory>
{
    Q_OBJECT
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
class ISCORE_LIB_BASE_EXPORT DocumentPluginFactory
    : public iscore::Interface<DocumentPluginFactory>
{
  ISCORE_INTERFACE("570faa0b-f100-4039-a2f0-b60347c4e581")
public:
  virtual ~DocumentPluginFactory();

  virtual DocumentPlugin* load(
      const VisitorVariant& var, iscore::DocumentContext& doc, QObject* parent)
      = 0;
};
class ISCORE_LIB_BASE_EXPORT DocumentPluginFactoryList final
    : public iscore::InterfaceList<iscore::DocumentPluginFactory>
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

  static UuidKey<iscore::DocumentPluginFactory> static_concreteKey()
  {
    return Metadata<ConcreteKey_k, T>::get();
  }

  UuidKey<iscore::DocumentPluginFactory>
  concreteKey() const noexcept final override
  {
    return Metadata<ConcreteKey_k, T>::get();
  }
};
}
