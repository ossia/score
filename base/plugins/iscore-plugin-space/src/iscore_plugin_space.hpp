#pragma once
#include <QObject>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/GUIApplicationContextPlugin_QtInterface.hpp>

class iscore_plugin_space:
        public QObject,
        public iscore::FactoryInterface_QtInterface,
        public iscore::GUIApplicationContextPlugin_QtInterface,
        public iscore::FactoryList_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID FactoryInterface_QtInterface_iid)
        Q_INTERFACES(
                iscore::FactoryInterface_QtInterface
                iscore::GUIApplicationContextPlugin_QtInterface
                iscore::FactoryList_QtInterface
                )

    public:
        iscore_plugin_space();
        virtual ~iscore_plugin_space();

        // Application plugin interface
        iscore::GUIApplicationContextPlugin* make_applicationPlugin(
                const iscore::ApplicationContext&) override;

        // Process & inspector
        std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> factories(
                const iscore::ApplicationContext& ctx,
                const iscore::FactoryBaseKey& matchingName) const override;

        std::vector<std::unique_ptr<iscore::FactoryListInterface>> factoryFamilies() override;
};
