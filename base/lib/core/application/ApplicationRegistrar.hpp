#pragma once
#include <vector>
#include <unordered_map>
#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

namespace iscore
{
class DocumentDelegateFactoryInterface;
class PluginControlInterface;
class PanelFactory;
class FactoryListInterface;
class PanelPresenter;
class Presenter;
struct ApplicationComponentsData;

class ApplicationRegistrar : public QObject
{
    public:
        ApplicationRegistrar(ApplicationComponentsData&, Presenter&);
        // Register data from plugins
        void registerPluginControl(PluginControlInterface*);
        void registerPanel(PanelFactory*);
        void registerDocumentDelegate(DocumentDelegateFactoryInterface*);
        void registerCommands(std::unordered_map<CommandParentFactoryKey, CommandGeneratorMap>&& cmds);
        void registerFactories(std::unordered_map<iscore::FactoryBaseKey, FactoryListInterface*>&& cmds);

    private:
        ApplicationComponentsData& m_components;
        Presenter& m_presenter;
};
}
