#pragma once
#include <iscore/command/CommandGeneratorMap.hpp>
#include <qobject.h>
#include <unordered_map>
#include <utility>

#include "iscore/command/SerializableCommand.hpp"
#include "iscore/plugins/customfactory/FactoryInterface.hpp"

namespace iscore {
class Application;
}  // namespace iscore

namespace iscore
{
class DocumentDelegateFactoryInterface;
class FactoryListInterface;
class GUIApplicationContextPlugin;
class PanelFactory;
class SettingsDelegateFactoryInterface;
struct ApplicationComponentsData;

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
