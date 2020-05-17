#pragma once
#include <score/model/IdentifiedObject.hpp>
#include <score/model/Identifier.hpp>
#include <score/selection/Selection.hpp>

#include <ossia/detail/json.hpp>

#include <QByteArray>
#include <QVariant>

#include <vector>
#include <verdigris>

namespace score
{
class DocumentDelegateFactory;
class DocumentDelegateModel;
class DocumentPlugin;
class Document;
struct ApplicationContext;

/**
 * @brief Model part of a document.
 *
 * Drawbridge between the application and a model given by a plugin.
 * Contains all the "saveable" data.
 */
class SCORE_LIB_BASE_EXPORT DocumentModel final : public IdentifiedObject<DocumentModel>
{
  W_OBJECT(DocumentModel)
  friend class Document;

public:
  DocumentModel(
      const Id<DocumentModel>& id,
      const score::DocumentContext& ctx,
      DocumentDelegateFactory& fact,
      QObject* parent);
  DocumentModel(QObject* parent);
  ~DocumentModel();

  DocumentDelegateModel& modelDelegate() const { return *m_model; }

  // Plugin models
  void addPluginModel(DocumentPlugin* m);
  const std::vector<DocumentPlugin*>& pluginModels() { return m_pluginModels; }

  void pluginModelsChanged() E_SIGNAL(SCORE_LIB_BASE_EXPORT, pluginModelsChanged)

private:
  void loadDocumentAsJson(
      score::DocumentContext& ctx,
      const rapidjson::Value&,
      DocumentDelegateFactory& fact);
  void loadDocumentAsByteArray(
      score::DocumentContext& ctx,
      const QByteArray&,
      DocumentDelegateFactory& fact);

  std::vector<DocumentPlugin*> m_pluginModels;
  DocumentDelegateModel* m_model{}; // note : this *has* to be last due to init order
};
}
