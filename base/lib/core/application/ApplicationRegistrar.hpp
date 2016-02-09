#pragma once
#include <iscore/command/CommandGeneratorMap.hpp>
#include <QObject>
#include <unordered_map>
#include <utility>

#include <iscore/command/SerializableCommand.hpp>
#include <iscore/plugins/customfactory/FactoryInterface.hpp>

#include <iscore_lib_base_export.h>
namespace iscore
{
class DocumentDelegateFactory;
class FactoryListInterface;
class GUIApplicationContextPlugin;
class PanelFactory;
class SettingsDelegateFactory;
struct ApplicationComponentsData;
class View;
class MenubarManager;
struct OrderedToolbar;
class Settings;
class Plugin_QtInterface;

class ISCORE_LIB_BASE_EXPORT ApplicationRegistrar : public QObject
{
    public:
        ApplicationRegistrar(
                ApplicationComponentsData&,
                const iscore::ApplicationContext&,
                iscore::View&,
                MenubarManager&,
                std::vector<OrderedToolbar>&,
                QObject* panelPresenterParent);

        // Register data from plugins
        void registerPlugins(const QStringList&, const std::vector<iscore::Plugin_QtInterface*>& vec);
        void registerApplicationContextPlugin(GUIApplicationContextPlugin*);
        void registerPanel(PanelFactory*);
        void registerCommands(std::unordered_map<CommandParentFactoryKey, CommandGeneratorMap>&& cmds);
        void registerCommands(std::pair<CommandParentFactoryKey, CommandGeneratorMap>&& cmds);
        void registerFactories(std::unordered_map<iscore::AbstractFactoryKey, std::unique_ptr<FactoryListInterface>>&& cmds);
        void registerFactory(std::unique_ptr<FactoryListInterface> cmds);

        auto& components() const
        { return m_components; }

    private:
        ApplicationComponentsData& m_components;
        const iscore::ApplicationContext& m_context;
        iscore::View& m_view;
        MenubarManager& m_menubar;
        std::vector<OrderedToolbar>& m_toolbars;
        QObject* m_panelPresenterParent{};
};
}
