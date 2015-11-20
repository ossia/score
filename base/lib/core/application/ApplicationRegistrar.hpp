#pragma once
#include <vector>
#include <unordered_map>
#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

namespace iscore
{
class DocumentDelegateFactoryInterface;
class GUIApplicationContextPlugin;
class PanelFactory;
class FactoryListInterface;
class PanelPresenter;
class Presenter;
struct ApplicationComponentsData;
class SettingsDelegateFactoryInterface;

class ApplicationRegistrar : public QObject
{
    public:
        ApplicationRegistrar(ApplicationComponentsData&, iscore::Application&);
        // Register data from plugins
        void registerApplicationContextPlugin(GUIApplicationContextPlugin*);
        void registerPanel(PanelFactory*);
        void registerDocumentDelegate(DocumentDelegateFactoryInterface*);
        void registerCommands(std::unordered_map<CommandParentFactoryKey, CommandGeneratorMap>&& cmds);
        void registerCommands(std::pair<CommandParentFactoryKey, CommandGeneratorMap>&& cmds);
        void registerFactories(std::unordered_map<iscore::FactoryBaseKey, FactoryListInterface*>&& cmds);
        void registerFactory(FactoryListInterface* cmds);
        void registerSettings(SettingsDelegateFactoryInterface*);

        auto& components() const
        { return m_components; }

    private:
        ApplicationComponentsData& m_components;
        Application& m_app;
};
}
