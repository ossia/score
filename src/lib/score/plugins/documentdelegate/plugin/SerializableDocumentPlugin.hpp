#pragma once
#include <score/plugins/documentdelegate/plugin/DocumentPluginBase.hpp>

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
 *   If there are informations that need to be reloaded **after**
 *   the DocumentModel was loaded. For instance components.
 *   This happens after the object has been constructed.
 */
class SCORE_LIB_BASE_EXPORT SerializableDocumentPlugin
    : public DocumentPlugin,
      public SerializableInterface<DocumentPluginFactory>
{
  W_OBJECT(SerializableDocumentPlugin)
public:

protected:
  using DocumentPlugin::DocumentPlugin;
  using ConcreteKey = UuidKey<DocumentPluginFactory>;

  virtual ~SerializableDocumentPlugin();
};

}
