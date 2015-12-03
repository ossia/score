#pragma once
#include <iscore/command/CommandGeneratorMap.hpp>
#include <QObject>
#include <unordered_map>
#include <utility>

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>

namespace iscore
{
class DocumentDelegateFactoryInterface;
class FactoryListInterface;
class GUIApplicationContextPlugin;
class PanelFactory;
class SettingsDelegateFactoryInterface;
struct ApplicationComponentsData;
class View;
class MenubarManager;
struct OrderedToolbar;
class Settings;

class ApplicationRegistrar : public QObject
{
    public:
        ApplicationRegistrar(
                ApplicationComponentsData&,
                const iscore::ApplicationContext&,
                iscore::View&,
                Settings&,
                MenubarManager&,
                std::vector<OrderedToolbar>&,
                QObject* panelPresenterParent);

        // Register data from plugins
        void registerPlugins(const QStringList&, const std::vector<QObject*>& vec);
        void registerApplicationContextPlugin(GUIApplicationContextPlugin*);
        void registerPanel(PanelFactory*);
        void registerDocumentDelegate(DocumentDelegateFactoryInterface*);
        void registerCommands(std::unordered_map<CommandParentFactoryKey, CommandGeneratorMap>&& cmds);
        void registerCommands(std::pair<CommandParentFactoryKey, CommandGeneratorMap>&& cmds);
        void registerFactories(std::unordered_map<iscore::FactoryBaseKey,std::unique_ptr<FactoryListInterface>>&& cmds);
        void registerFactory(std::unique_ptr<FactoryListInterface> cmds);
        void registerSettings(SettingsDelegateFactoryInterface*);

        auto& components() const
        { return m_components; }

    private:
        ApplicationComponentsData& m_components;
        const iscore::ApplicationContext& m_context;
        iscore::View& m_view;
        Settings& m_settings;
        MenubarManager& m_menubar;
        std::vector<OrderedToolbar>& m_toolbars;
        QObject* m_panelPresenterParent{};
};
}
