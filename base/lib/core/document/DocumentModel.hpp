#pragma once
#include <iscore/selection/Selection.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include <QByteArray>
#include <QJsonObject>

#include <QString>
#include <QVariant>
#include <algorithm>
#include <iterator>
#include <vector>

class QObject;
#include <iscore/tools/SettableIdentifier.hpp>

namespace iscore
{
class DocumentDelegateFactoryInterface;
class DocumentDelegateModelInterface;
class DocumentPluginModel;
class PanelModel;
struct ApplicationContext;

/**
     * @brief The DocumentModel class
     *
     * Drawbridge between the application and a model given by a plugin.
     * Contains all the "saveable" data.
     */
class ISCORE_LIB_BASE_EXPORT DocumentModel final : public IdentifiedObject<DocumentModel>
{
        Q_OBJECT
    public:
        DocumentModel(
                const Id<DocumentModel>& id,
                DocumentDelegateFactoryInterface* fact,
                QObject* parent);
        DocumentModel(
                const iscore::ApplicationContext& ctx,
                const QVariant &data,
                DocumentDelegateFactoryInterface* fact,
                QObject* parent);
        ~DocumentModel();

        DocumentDelegateModelInterface& modelDelegate() const
        {
            return *m_model;
        }


        // Panel models
        void addPanel(PanelModel* m);
        const auto& panels() const { return m_panelModels; }

        template<typename T>
        T* panel() const
        {
            using namespace std;
            auto it = find_if(begin(m_panelModels),
                              end(m_panelModels),
                              [&](PanelModel * pm)
            { return dynamic_cast<T*>(pm); });

            return it != end(m_panelModels) ? safe_cast<T*>(*it) : nullptr;
        }

        // Plugin models
        void addPluginModel(DocumentPluginModel* m);
        const std::vector<DocumentPluginModel*>& pluginModels() { return m_pluginModels; }

        QString docFileName() const;
        void setDocFileName(const QString &docFileName);

signals:
        void pluginModelsChanged();
        void fileNameChanged(const QString&);

    public slots:
        void setNewSelection(const Selection&);

    private:
        void loadDocumentAsJson(
                const iscore::ApplicationContext& ctx,
                const QJsonObject&,
                DocumentDelegateFactoryInterface* fact);
        void loadDocumentAsByteArray(
                const iscore::ApplicationContext& ctx,
                const QByteArray&,
                DocumentDelegateFactoryInterface* fact);

        QString m_docFileName{tr("Untitled")};

        std::vector<PanelModel*> m_panelModels;
        std::vector<DocumentPluginModel*> m_pluginModels;
        DocumentDelegateModelInterface* m_model{}; // note : this *has* to be last due to init order
};
}
