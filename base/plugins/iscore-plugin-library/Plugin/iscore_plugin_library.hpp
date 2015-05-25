#pragma once
#include <iscore/plugins/qt_interfaces/PanelFactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginControlInterface_QtInterface.hpp>
class iscore_plugin_library :
    public QObject,
    public iscore::PanelFactory_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID PanelFactory_QtInterface_iid)
        Q_INTERFACES(
                     iscore::PanelFactory_QtInterface)

    public:
        iscore_plugin_library();

        QList<iscore::PanelFactory*> panels() override;


};
