#pragma once
#include <public_interface/tools/NamedObject.hpp>
#include <core/presenter/Presenter.hpp>

namespace iscore
{
    class SerializableCommand;
    class Presenter;
    class MenubarManager;

    /**
     * @brief The PluginControlInterface class
     *
     * This class's goal is to :
     * * Instantiate some elements that are deeply intertwined with Qt : menus, toolbars
     * * Manage the Commands of the plug-in : it has to be able to instantiate any meaningful
     *   Command, if received by the network.
     *
     * It is instatiated exactly once by the Presenter class in i-score.
     */
    class PluginControlInterface : public NamedObject
    {
            Q_OBJECT
            // Menus : trouver un moyen pour créer automatiquement si n'existe pas ?
            // Dire chemin : File/Export/SomeCustomExport ?
            // Pb. : traduction ? (ex. : soft traduit & plug pas traduit ?)
            // Fournir menus de base : Fichier Edition Affichage Objet Arrangement Devices Fenêtre Paramètres Aide
        public:
            using NamedObject::NamedObject;
            virtual ~PluginControlInterface() = default;
            virtual void populateMenus(iscore::MenubarManager*) = 0;
            virtual void populateToolbars() = 0;

            Presenter* presenter() const
            {
                return m_presenter;
            }

            void setPresenter(Presenter* p)
            {
                m_presenter = p;
                on_presenterChanged();
            }

            virtual SerializableCommand* instantiateUndoCommand(
                    const QString& name,
                    const QByteArray& data)
            {
                return nullptr;
            }

            Document* currentDocument() const
            {
                return m_presenter->currentDocument();
            }

            virtual void on_newDocument(iscore::Document* doc) {}

        protected:
            virtual void on_presenterChanged() {}

        private:
            Presenter* m_presenter{};
    };

}
