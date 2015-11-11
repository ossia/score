#include "iscore_plugin_space.hpp"

#include "SpaceProcessFactory.hpp"
#include "SpaceControl.hpp"
#include "Area/AreaFactory.hpp"
#include "Area/SingletonAreaFactoryList.hpp"

#include "Area/Circle/CircleAreaFactory.hpp"
#include "Area/Generic/GenericAreaFactory.hpp"
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

iscore_plugin_space::iscore_plugin_space() :
    QObject {}
{
}

iscore::PluginControlInterface* iscore_plugin_space::make_control(iscore::Presenter* pres)
{
    return new SpaceControl{pres};
}

std::vector<iscore::FactoryInterface*> iscore_plugin_space::factories(const QString& factoryName)
{
    if(factoryName == ProcessFactory::factoryName())
    {
        return {new SpaceProcessFactory};
    }
    if(factoryName == AreaFactory::factoryName())
    {
        return {
            new GenericAreaFactory,
                    new CircleAreaFactory
        };
    }

    return {};
}


QVector<iscore::FactoryFamily> iscore_plugin_space::factoryFamilies()
{
    return {{"Area",
            [] (iscore::FactoryInterface* f)
            { SingletonAreaFactoryList::instance().registerFactory(f); }}};
}
