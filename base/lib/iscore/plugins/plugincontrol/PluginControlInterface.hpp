#pragma once
#include <iscore/tools/NamedObject.hpp>
#include <core/presenter/Presenter.hpp>
#include <iscore/widgets/OrderedToolbar.hpp>
class QToolBar;

namespace iscore
{
    class DocumentDelegatePluginModel;
    class SerializableCommand;
    class Application;
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
            // Menus : trouver un moyen pour créer automatiquement si n'existe pas ?
            // Dire chemin : File/Export/SomeCustomExport ?
            // Pb. : traduction ? (ex. : soft traduit & plug pas traduit ?)
            // Fournir menus de base : Fichier Edition Affichage Objet Arrangement Devices Fenêtre Paramètres Aide
        Q_OBJECT
        public:
            PluginControlInterface(iscore::Application& presenter,
                                   const QString& name,
                                   QObject* parent);

            virtual ~PluginControlInterface();

            virtual void populateMenus(iscore::MenubarManager*);
            virtual QList<iscore::OrderedToolbar> makeToolbars();
            virtual QList<QAction*> actions();

            virtual DocumentDelegatePluginModel* loadDocumentPlugin(
                    const QString& name,
                    const VisitorVariant& var,
                    iscore::Document *parent);

            const ApplicationContext& context() const;
            Document* currentDocument() const;

            virtual void on_newDocument(iscore::Document* doc);
            virtual void on_loadedDocument(iscore::Document* doc);
            virtual void prepareNewDocument();

        private slots:
            void on_focusChanged(Qt::ApplicationState st);

        signals:
            void documentChanged(
                    iscore::Document* olddoc,
                    iscore::Document* newdoc);

            void defocused();
            void focused();

        protected:
            virtual void on_documentChanged(
                    iscore::Document* olddoc,
                    iscore::Document* newdoc);

        private:
            ApplicationContext m_appContext;
    };

}
