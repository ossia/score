#pragma once
#include <core/presenter/MenubarManager.hpp>

#include <set>
#include <core/document/Document.hpp>
#include <core/document/DocumentBuilder.hpp>

#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/ObjectPath.hpp>

#include <iscore/widgets/OrderedToolbar.hpp>
#include <QRecentFilesMenu.h>

#include <iscore/command/CommandGeneratorMap.hpp>
#include <unordered_map>

namespace iscore
{
    class SerializableCommand;
    class PluginControlInterface;
    class View;
    class PanelFactory;
    class PanelPresenter;
    /**
     * @brief The Presenter class
     *
     * Certainly needs refactoring.
     * For now, manages menus and plug-in objects.
     *
     * It is also able to instantiate a Command from serialized Undo/Redo data.
     * (this should go in the DocumentPresenter maybe ?)
     */
    class Presenter final : public NamedObject
    {
            Q_OBJECT
        public:
            Presenter(iscore::View* view, QObject* parent);
            ~Presenter();

            // Register data from plugins
            void registerPluginControl(PluginControlInterface*);
            void registerPanel(PanelFactory*);
            void registerDocumentDelegate(DocumentDelegateFactoryInterface*);
            void registerCommands(std::unordered_map<std::string, CommandGeneratorMap>&& cmds)
            {
                m_commands = std::move(cmds);
            }

            // Getters for plugin-registered things
            const std::vector<PluginControlInterface*>& pluginControls() const;
            const std::vector<DocumentDelegateFactoryInterface *> &availableDocuments() const;

            // Document management
            Document* setupDocument(iscore::Document* doc);

            template<typename... Args>
            void newDocument(Args&&... args)
            {
                emit prepareNewDocument();
                setupDocument(m_builder.newDocument(std::forward<Args>(args)...));
            }

            template<typename... Args>
            Document* loadDocument(Args&&... args)
            {
                emit prepareNewDocument();
                return setupDocument(m_builder.loadDocument(std::forward<Args>(args)...));
            }

            template<typename... Args>
            void restoreDocument(Args&&... args)
            {
                emit prepareNewDocument();
                setupDocument(m_builder.restoreDocument(std::forward<Args>(args)...));
            }

            // Restore documents after a crash
            void restoreDocuments();

            const QList<Document*>& documents() const
            { return m_documents; }

            Document* currentDocument() const;
            void setCurrentDocument(Document* doc);

            // Returns true if the document was closed.
            bool closeDocument(Document*);

            // Exit i-score
            bool exit();


            // Methods to save and load
            bool saveDocument(Document*);
            bool saveDocumentAs(Document*);

            Document* loadFile();
            Document* loadFile(const QString& filename);

            // Toolbars
            QList<OrderedToolbar>& toolbars()
            { return m_toolbars; }

            /**
             * @brief instantiateUndoCommand Is used to generate a Command from its serialized data.
             * @param parent_name The name of the object able to generate the command. Must be a CustomCommand.
             * @param name The name of the command to generate.
             * @param data The data of the command.
             *
             * Ownership of the command is transferred to the caller, and he must delete it.
             */
            iscore::SerializableCommand*
            instantiateUndoCommand(const std::string& parent_name,
                                   const std::string& name,
                                   const QByteArray& data);

            QList<PanelFactory*> panelFactories() const
            {
                QList<PanelFactory*> lst;
                std::transform(
                            std::begin(m_panelPresenters),
                            std::end(m_panelPresenters),
                            std::back_inserter(lst),
                    [] (const QPair<PanelPresenter*, PanelFactory*>& elt) {
                        return elt.second;
                    }
                );
                return lst;
            }

            const std::vector<PluginControlInterface*>& controls() const;
            View* view() const;

        signals:
            void prepareNewDocument();
            void currentDocumentChanged(Document* newDoc);

        private:
            void setupMenus();

            View* m_view {};
            MenubarManager m_menubar;
            QList<Document*> m_documents{};
            Document* m_currentDocument{};

            std::vector<PluginControlInterface*> m_controls;
            std::vector<DocumentDelegateFactoryInterface*> m_availableDocuments;
            std::unordered_map<std::string, CommandGeneratorMap> m_commands;

            // TODO instead put the factory as a member function?
            QList<QPair<PanelPresenter*,
                        PanelFactory*>> m_panelPresenters;

            QList<OrderedToolbar> m_toolbars;

            DocumentBuilder m_builder{*this};

            QRecentFilesMenu* m_recentFiles{};

    };
}
