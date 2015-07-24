#include "iscore_plugin_space.hpp"

#include "SpaceProcessFactory.hpp"
#include "SpaceControl.hpp"
iscore_plugin_space::iscore_plugin_space() :
    QObject {}
{
}

iscore::PluginControlInterface* iscore_plugin_space::make_control(iscore::Presenter* pres)
{
    return new SpaceControl{pres};
}

QVector<iscore::FactoryInterface*> iscore_plugin_space::factories(const QString& factoryName)
{
    if(factoryName == ProcessFactory::factoryName())
    {
        return {new SpaceProcessFactory};
    }

    return {};
}
