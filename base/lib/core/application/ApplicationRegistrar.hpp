#pragma once
#include <vector>
#include <unordered_map>
#include <iscore/command/CommandGeneratorMap.hpp>

namespace iscore
{
class DocumentDelegateFactoryInterface;
class PluginControlInterface;
class PanelFactory;
class PanelPresenter;
class Presenter;

class ApplicationRegistrar : public QObject
{
    public:
        ApplicationRegistrar(Presenter&);
        // Register data from plugins
        void registerPluginControl(PluginControlInterface*);
        void registerPanel(PanelFactory*);
        void registerDocumentDelegate(DocumentDelegateFactoryInterface*);
        void registerCommands(std::unordered_map<CommandParentFactoryKey, CommandGeneratorMap>&& cmds);

        // Getters for plugin-registered things
        const auto& pluginControls() const
        { return m_controls; }
        const auto& availableDocuments() const
        { return m_availableDocuments; }
        const auto& controls() const
        { return m_controls; }
        const auto& panelPresenters() const
        { return m_panelPresenters; }
        auto panelFactories() const
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


        /**
         * @brief instantiateUndoCommand Is used to generate a Command from its serialized data.
         * @param parent_name The name of the object able to generate the command. Must be a CustomCommand.
         * @param name The name of the command to generate.
         * @param data The data of the command.
         *
         * Ownership of the command is transferred to the caller, and he must delete it.
         */
        iscore::SerializableCommand*
        instantiateUndoCommand(const CommandParentFactoryKey& parent_name,
                               const CommandFactoryKey& name,
                               const QByteArray& data);


    private:
        Presenter& m_presenter;
        std::vector<PluginControlInterface*> m_controls;
        std::vector<DocumentDelegateFactoryInterface*> m_availableDocuments;
        std::unordered_map<CommandParentFactoryKey, CommandGeneratorMap> m_commands;

        // TODO instead put the factory as a member function?
        QList<QPair<PanelPresenter*,
                    PanelFactory*>> m_panelPresenters;


};
}
