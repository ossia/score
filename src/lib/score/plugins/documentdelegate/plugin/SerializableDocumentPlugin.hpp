#pragma once
#include <score/plugins/documentdelegate/plugin/DocumentPluginBase.hpp>

#include <QByteArray>

#include <score/serialization/OpaquePayload.hpp>

#include <verdigris>

namespace score
{
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
 *   If there is information that needs to be reloaded **after**
 *   the DocumentModel was loaded. For instance components.
 *   This happens after the object has been constructed.
 */
class SCORE_LIB_BASE_EXPORT SerializableDocumentPlugin
    : public DocumentPlugin
    , public SerializableInterface<DocumentPluginFactory>
{
  W_OBJECT(SerializableDocumentPlugin)
public:
protected:
  using DocumentPlugin::DocumentPlugin;
  using ConcreteKey = UuidKey<DocumentPluginFactory>;

  virtual ~SerializableDocumentPlugin();
};

/**
 * @brief Stands in for a document plug-in this build does not have.
 *
 * Document plug-ins carry whole subsystems' worth of state -- the network
 * add-on keeps its groups in one -- and there was nowhere to put that when the
 * plug-in was absent, so it was dropped and saving wrote the document back
 * without it. Keeping it means a session document opened by a peer without the
 * add-on still describes its groups when it gets back to one that has it.
 */
class SCORE_LIB_BASE_EXPORT OpaqueDocumentPlugin final
    : public SerializableDocumentPlugin
{
  W_OBJECT(OpaqueDocumentPlugin)
public:
  OpaqueDocumentPlugin(
      const UuidKey<DocumentPluginFactory>& key, const score::DocumentContext& ctx,
      DataStream::Deserializer& vis, QObject* parent);
  OpaqueDocumentPlugin(
      const UuidKey<DocumentPluginFactory>& key, const score::DocumentContext& ctx,
      JSONObject::Deserializer& vis, QObject* parent);
  ~OpaqueDocumentPlugin() override;

  //! The key of the plug-in we replace, so that saving names it and not us.
  UuidKey<DocumentPluginFactory> concreteKey() const noexcept override { return m_key; }
  void serialize_impl(const VisitorVariant& vis) const noexcept override;

private:
  UuidKey<DocumentPluginFactory> m_key;
  score::OpaquePayload m_payload;
};

}
