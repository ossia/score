#pragma once
#include <core/presenter/MenubarManager.hpp>

#include <set>
#include <core/document/Document.hpp>

#include <tools/NamedObject.hpp>
#include <tools/ObjectPath.hpp>

namespace iscore
{
    class SerializableCommand;
    class PluginControlInterface;
    class Model;
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
            Presenter(iscore::Model* model, iscore::View* view, QObject* parent);

            void registerPluginControl(PluginControlInterface*);
            void registerPanel(PanelFactoryInterface*);
            void registerDocumentPanel(DocumentDelegateFactoryInterface*);

            Document* currentDocument() const;
            void setCurrentDocument(Document* doc);

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

        signals:
            void currentDocumentChanged(Document* newDoc);

        public slots:
            /**
             * @brief newDocument Create a new document.
             */
            void newDocument(iscore::DocumentDelegateFactoryInterface* doctype);

            void on_lock(QByteArray);
            void on_unlock(QByteArray);
        private:
            void setupMenus();

            Model* m_model {};
            View* m_view {};
            MenubarManager m_menubar;
            QList<Document*> m_documents{};
            Document* m_currentDocument{};

            std::vector<PluginControlInterface*> m_customControls;
            std::vector<DocumentDelegateFactoryInterface*> m_availableDocuments;

            // TODO instead put the factory as a member function?
            QList<QPair<PanelPresenterInterface*,
                        PanelFactoryInterface*>> m_panelPresenters;
    };
}
