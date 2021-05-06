#pragma once
#include <score/document/DocumentContext.hpp>
#include <score/model/IdentifiedObject.hpp>
#include <score/plugins/SerializableInterface.hpp>
#include <score/serialization/VisitorCommon.hpp>

#include <QString>

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
    : public QObject
{
  W_OBJECT(DocumentPlugin)
public:
  DocumentPlugin(
      const score::DocumentContext&,
      const QString& name,
      QObject* parent);

  virtual ~DocumentPlugin();

  const score::DocumentContext& context() const { return m_context; }

  template <typename Impl>
  explicit DocumentPlugin(
      const score::DocumentContext& ctx,
      Impl& vis,
      QObject* parent)
      : QObject{parent}
      , m_context{ctx}
  {
  }

  virtual void on_documentClosing();

protected:
  const score::DocumentContext& m_context;
};

class DocumentPluginFactory;
}
