#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QVariant>
#include <algorithm>
#include <iterator>
#include <score/model/IdentifiedObject.hpp>
#include <score/selection/Selection.hpp>
#include <vector>
#include <wobjectdefs.h>
class QObject;
#include <score/model/Identifier.hpp>

namespace score
{
class DocumentDelegateFactory;
class DocumentDelegateModel;
class DocumentPlugin;
struct ApplicationContext;

/**
 * @brief Model part of a document.
 *
 * Drawbridge between the application and a model given by a plugin.
 * Contains all the "saveable" data.
 */
class SCORE_LIB_BASE_EXPORT DocumentModel final
    : public IdentifiedObject<DocumentModel>
{
  W_OBJECT(DocumentModel)
public:
  DocumentModel(
      const Id<DocumentModel>& id,
      const score::DocumentContext& ctx,
      DocumentDelegateFactory& fact,
      QObject* parent);
  DocumentModel(
      score::DocumentContext& ctx,
      const QVariant& data,
      DocumentDelegateFactory& fact,
      QObject* parent);
  ~DocumentModel();

  DocumentDelegateModel& modelDelegate() const
  {
    return *m_model;
  }

  // Plugin models
  void addPluginModel(DocumentPlugin* m);
  const std::vector<DocumentPlugin*>& pluginModels()
  {
    return m_pluginModels;
  }

  void pluginModelsChanged() W_SIGNAL(pluginModelsChanged)

      private
      : void loadDocumentAsJson(
            score::DocumentContext& ctx,
            const QJsonObject&,
            DocumentDelegateFactory& fact);
  void loadDocumentAsByteArray(
      score::DocumentContext& ctx,
      const QByteArray&,
      DocumentDelegateFactory& fact);

  std::vector<DocumentPlugin*> m_pluginModels;
  DocumentDelegateModel*
      m_model{}; // note : this *has* to be last due to init order
};
}
