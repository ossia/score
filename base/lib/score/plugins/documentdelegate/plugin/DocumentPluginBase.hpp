#pragma once
#include <QString>
#include <score/document/DocumentContext.hpp>
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
  W_OBJECT(DocumentPlugin)
public:
  DocumentPlugin(
      const score::DocumentContext&,
      Id<DocumentPlugin> id,
      const QString& name,
      QObject* parent);

  virtual ~DocumentPlugin();

  const score::DocumentContext& context() const
  {
    return m_context;
  }

  template <typename Impl>
  explicit DocumentPlugin(
      const score::DocumentContext& ctx, Impl& vis, QObject* parent)
      : IdentifiedObject{vis, parent}, m_context{ctx}
  {
  }

  virtual void on_documentClosing();

protected:
  const score::DocumentContext& m_context;
};

class DocumentPluginFactory;
}
