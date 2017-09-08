#pragma once
#include <score/document/DocumentContext.hpp>

#include <QString>
#include <score/plugins/customfactory/SerializableInterface.hpp>
#include <score/serialization/VisitorCommon.hpp>
#include <vector>

class QWidget;
namespace score
{
class Document;
}

// TODO DocumentPlugin -> system
namespace score
{
/**
 * @brief Extend a document with custom data and systems.
 */
class SCORE_LIB_BASE_EXPORT DocumentPlugin
    : public IdentifiedObject<DocumentPlugin>
{
  Q_OBJECT
public:
  DocumentPlugin(
      const score::DocumentContext&,
      Id<DocumentPlugin>
          id,
      const QString& name,
      QObject* parent);

  virtual ~DocumentPlugin();

  const score::DocumentContext& context() const
  {
    return m_context;
  }

  template <typename Impl>
  explicit DocumentPlugin(
      const score::DocumentContext& ctx,
      Impl& vis,
      QObject* parent)
      : IdentifiedObject{vis, parent}, m_context{ctx}
  {
  }

  virtual void on_documentClosing();
protected:
  const score::DocumentContext& m_context;
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
class SCORE_LIB_BASE_EXPORT SerializableDocumentPlugin
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
class SCORE_LIB_BASE_EXPORT DocumentPluginFactory
    : public score::Interface<DocumentPluginFactory>
{
  SCORE_INTERFACE("570faa0b-f100-4039-a2f0-b60347c4e581")
public:
  virtual ~DocumentPluginFactory();

  virtual DocumentPlugin* load(
      const VisitorVariant& var, score::DocumentContext& doc, QObject* parent)
      = 0;
};
class SCORE_LIB_BASE_EXPORT DocumentPluginFactoryList final
    : public score::InterfaceList<score::DocumentPluginFactory>
{
public:
  using object_type = DocumentPlugin;
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

  UuidKey<score::DocumentPluginFactory>
  concreteKey() const noexcept final override
  {
    return Metadata<ConcreteKey_k, T>::get();
  }
};
}
