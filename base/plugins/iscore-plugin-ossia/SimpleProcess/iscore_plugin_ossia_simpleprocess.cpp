#include "iscore_plugin_ossia_simpleprocess.hpp"

#include "SimpleProcessFactory.hpp"

iscore_plugin_ossia_simpleprocess::iscore_plugin_ossia_simpleprocess() :
    QObject {}
{

}

iscore_plugin_ossia_simpleprocess::~iscore_plugin_ossia_simpleprocess()
{

}


QVector<iscore::FactoryInterface*> iscore_plugin_ossia_simpleprocess::factories(const QString& factoryName)
{
    if(factoryName == ProcessFactory::factoryName())
    {
        return {new SimpleProcessFactory};
    }

    return {};
}

