#pragma once
#include <tools/NamedObject.hpp>
#include <set>

namespace iscore
{
    class DocumentDelegateModelInterface;
    class PanelModelInterface;
    /**
     * @brief The DocumentDelegateModelInterface class
     *
     * Drawbridge between the application and a model given by a plugin.
     */
    class DocumentModel : public NamedObject
    {
        public:
            DocumentModel (QObject* parent);
            void reset();
            void setModelDelegate (DocumentDelegateModelInterface* m);
            DocumentDelegateModelInterface* modelDelegate() const
            {
                return m_model;
            }

            void addPanel (PanelModelInterface* m)
            {
                m_panelModels.insert (m);
            }
            const std::set<PanelModelInterface*>& panels() const
            {
                return m_panelModels;
            }

            // Returns a Panel by name.
            PanelModelInterface* panel (QString name) const;

        private:
            DocumentDelegateModelInterface* m_model {};
            std::set<PanelModelInterface*> m_panelModels;
    };
}
