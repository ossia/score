#pragma once
#include <QObject>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/FactoryFamily_QtInterface.hpp>
#include <iscore/plugins/qt_interfaces/PluginControlInterface_QtInterface.hpp>

class iscore_plugin_ossia_simpleprocess:
    public QObject,
    public iscore::FactoryInterface_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID FactoryInterface_QtInterface_iid)
        Q_INTERFACES(
            iscore::FactoryInterface_QtInterface
        )

    public:
        iscore_plugin_ossia_simpleprocess();
        virtual ~iscore_plugin_ossia_simpleprocess();

        // Process & inspector
        virtual QVector<iscore::FactoryInterface*> factories(const QString& factoryName) override;
};
