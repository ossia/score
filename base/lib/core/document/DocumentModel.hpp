#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/selection/Selection.hpp>

namespace iscore
{
    class DocumentDelegateFactoryInterface;
    class DocumentDelegateModelInterface;
    class DocumentDelegatePluginModel;
    class PanelModelInterface;
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
            DocumentModel(QVariant data,
                          DocumentDelegateFactoryInterface* fact,
                          QObject* parent);

            DocumentDelegateModelInterface* modelDelegate() const
            {
                return m_model;
            }


            // Panel models
            void addPanel(PanelModelInterface* m);
            const auto& panels() const { return m_panelModels; }
            PanelModelInterface* panel(const QString& name) const;

            // Plugin models
            void addPluginModel(DocumentDelegatePluginModel* m);
            const auto& pluginModels() { return m_pluginModels; }
            DocumentDelegatePluginModel* pluginModel(const QString& name) const;

        signals:
            void pluginModelsChanged();

        public slots:
            void setNewSelection(const Selection&);

        private:
            QList<PanelModelInterface*> m_panelModels;
            QList<DocumentDelegatePluginModel*> m_pluginModels;
            DocumentDelegateModelInterface* m_model{}; // note : this *has* to be last due to init order
    };
}
