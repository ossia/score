#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <iscore/selection/Selection.hpp>
#include <iscore/model/IdentifiedObject.hpp>

#include <QString>
#include <QVariant>
#include <algorithm>
#include <iterator>
#include <vector>

class QObject;
#include <iscore/model/Identifier.hpp>

namespace iscore
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
class ISCORE_LIB_BASE_EXPORT DocumentModel final
    : public IdentifiedObject<DocumentModel>
{
  Q_OBJECT
public:
  DocumentModel(
      const Id<DocumentModel>& id,
      const iscore::DocumentContext& ctx,
      DocumentDelegateFactory& fact,
      QObject* parent);
  DocumentModel(
      iscore::DocumentContext& ctx,
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

  void setNewSelection(const Selection&);

signals:
  void pluginModelsChanged();

private:
  void loadDocumentAsJson(
      iscore::DocumentContext& ctx,
      const QJsonObject&,
      DocumentDelegateFactory& fact);
  void loadDocumentAsByteArray(
      iscore::DocumentContext& ctx,
      const QByteArray&,
      DocumentDelegateFactory& fact);

  std::vector<DocumentPlugin*> m_pluginModels;
  DocumentDelegateModel*
      m_model{}; // note : this *has* to be last due to init order
};
}
