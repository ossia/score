#include "iscore_plugin_ossia_simpleprocess.hpp"

#include "SimpleProcessFactory.hpp"

iscore_plugin_ossia_simpleprocess::iscore_plugin_ossia_simpleprocess() :
    QObject {}
{

}

iscore_plugin_ossia_simpleprocess::~iscore_plugin_ossia_simpleprocess()
{

}


std::vector<iscore::FactoryInterfaceBase*> iscore_plugin_ossia_simpleprocess::factories(const std::string& factoryName) const
{
    if(factoryName == ProcessFactory::staticFactoryName())
    {
        return {new SimpleProcessFactory};
    }

    return {};
}

