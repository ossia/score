#pragma once
#include <iscore/plugins/qt_interfaces/PluginRequirements_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/GUIApplicationPlugin_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/CommandFactory_QtInterface.hpp>

#include <iscore/application/ApplicationContext.hpp>
#include <iscore/command/CommandGeneratorMap.hpp>
#include <iscore/command/Command.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

#include <QObject>
#include <utility>
#include <vector>

class iscore_addon_skeleton final :
        public QObject,
        public iscore::Plugin_QtInterface,
        public iscore::FactoryInterface_QtInterface,
        public iscore::CommandFactory_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID FactoryInterface_QtInterface_iid)
        Q_INTERFACES(
                iscore::Plugin_QtInterface
                iscore::FactoryInterface_QtInterface
                iscore::CommandFactory_QtInterface
                )

  ISCORE_PLUGIN_METADATA(1, "00000000-0000-0000-0000-000000000000")

    public:
        iscore_addon_skeleton();
        virtual ~iscore_addon_skeleton();

    private:
        std::vector<std::unique_ptr<iscore::InterfaceBase>> factories(
                const iscore::ApplicationContext& ctx,
                const iscore::InterfaceKey& key) const override;

        std::pair<const CommandGroupKey, CommandGeneratorMap> make_commands() override;
};
