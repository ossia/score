#pragma once
#include <QObject>
#include <iscore/plugins/qt_interfaces/FactoryInterface_QtInterface.hpp>

class StatePlugin:
    public QObject,
    public iscore::FactoryInterface_QtInterface
{
        Q_OBJECT
        Q_PLUGIN_METADATA(IID FactoryInterface_QtInterface_iid)
        Q_INTERFACES(
            iscore::FactoryInterface_QtInterface
        )

    public:
        StatePlugin();
        virtual ~StatePlugin() = default;

        // Inspector
        virtual QVector<iscore::FactoryInterface*> factories_make(QString factoryName) override;
};

