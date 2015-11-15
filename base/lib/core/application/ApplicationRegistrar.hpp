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

    private:
        ApplicationComponentsData& m_components;
        Presenter& m_presenter;
};
}
