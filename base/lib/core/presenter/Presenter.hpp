#pragma once
#include <core/presenter/MenubarManager.hpp>

#include <set>
#include <core/document/Document.hpp>

#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/ObjectPath.hpp>

namespace iscore
{
    class SerializableCommand;
    class PluginControlInterface;
    class View;
    class PanelFactoryInterface;
    class PanelPresenterInterface;
    /**
     * @brief The Presenter class
     *
     * Certainly needs refactoring.
     * For now, manages menus and plug-in objects.
     *
     * It is also able to instantiate a Command from serialized Undo/Redo data.
     * (this should go in the DocumentPresenter maybe ?)
     */
    class Presenter : public NamedObject
    {
            Q_OBJECT
        public:
            Presenter(iscore::View* view, QObject* parent);

            // Register data from plugins
            void registerPluginControl(PluginControlInterface*);
            void registerPanel(PanelFactoryInterface*);
            void registerDocumentPanel(DocumentDelegateFactoryInterface*);

            // Getters for plugin-registered things
            const std::vector<PluginControlInterface*>& pluginControls() const;
            const std::vector<DocumentDelegateFactoryInterface *> &availableDocuments() const;

            // Document management
            void newDocument(iscore::DocumentDelegateFactoryInterface* doctype);
            Document* loadDocument(const QVariant &data,
                              iscore::DocumentDelegateFactoryInterface* doctype);

            Document* currentDocument() const;
            void setCurrentDocument(Document* doc);

            void closeDocument(Document*);

            // Methods to save and load
            void saveBinary(Document*);
            bool saveJson(Document*);

            void loadBinary();
            void loadJson();

            /**
             * @brief instantiateUndoCommand Is used to generate a Command from its serialized data.
             * @param parent_name The name of the object able to generate the command. Must be a CustomCommand.
             * @param name The name of the command to generate.
             * @param data The data of the command.
             *
             * Ownership of the command is transferred to the caller, and he must delete it.
             */
            iscore::SerializableCommand*
            instantiateUndoCommand(const QString& parent_name,
                                   const QString& name,
                                   const QByteArray& data);

            QList<PanelFactoryInterface*> panelFactories() const
            {
                using namespace std;
                QList<PanelFactoryInterface*> lst;
                transform(begin(m_panelPresenters), end(m_panelPresenters),
                          back_inserter(lst),
                          [] (const QPair<PanelPresenterInterface*,
                        PanelFactoryInterface*>& elt) { return elt.second; });
                return lst;
            }

        signals:
            void currentDocumentChanged(Document* newDoc);

        private:
            void setupMenus();

            View* m_view {};
            MenubarManager m_menubar;
            QList<Document*> m_documents{};
            Document* m_currentDocument{};

            std::vector<PluginControlInterface*> m_controls;
            std::vector<DocumentDelegateFactoryInterface*> m_availableDocuments;

            // TODO instead put the factory as a member function?
            QList<QPair<PanelPresenterInterface*,
                        PanelFactoryInterface*>> m_panelPresenters;
    };
}
