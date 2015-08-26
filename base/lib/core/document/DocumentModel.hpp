#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/selection/Selection.hpp>

namespace iscore
{
    class DocumentDelegateFactoryInterface;
    class DocumentDelegateModelInterface;
    class DocumentDelegatePluginModel;
    class PanelModel;
    /**
     * @brief The DocumentDelegateModelInterface class
     *
     * Drawbridge between the application and a model given by a plugin.
     */
    class DocumentModel : public IdentifiedObject<DocumentModel>
    {
            Q_OBJECT
        public:
            DocumentModel(DocumentDelegateFactoryInterface* fact,
                          QObject* parent);
            DocumentModel(const QVariant &data,
                          DocumentDelegateFactoryInterface* fact,
                          QObject* parent);

            DocumentDelegateModelInterface* modelDelegate() const
            {
                return m_model;
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
                { return qobject_cast<T*>(pm); });

                return it != end(m_panelModels) ? static_cast<T*>(*it) : nullptr;
            }

            // Plugin models
            void addPluginModel(DocumentDelegatePluginModel* m);
            const QList<DocumentDelegatePluginModel*>& pluginModels() { return m_pluginModels; }

            template<typename T>
            T* pluginModel() const
            {
                using namespace std;
                auto it = find_if(begin(m_pluginModels),
                                  end(m_pluginModels),
                                  [&](DocumentDelegatePluginModel * pm)
                { return qobject_cast<T*>(pm); });

                return it != end(m_pluginModels) ? static_cast<T*>(*it) : nullptr;
            }

        signals:
            void pluginModelsChanged();

        public slots:
            void setNewSelection(const Selection&);

        private:
            void loadDocumentAsJson(
                    const QJsonObject&,
                    DocumentDelegateFactoryInterface* fact);
            void loadDocumentAsByteArray(
                    const QByteArray&,
                    DocumentDelegateFactoryInterface* fact);

            QList<PanelModel*> m_panelModels;
            QList<DocumentDelegatePluginModel*> m_pluginModels;
            DocumentDelegateModelInterface* m_model{}; // note : this *has* to be last due to init order
    };
}
