#include "Process/ProcessFactory.hpp"
#include "SimpleProcessFactory.hpp"
#include "iscore/plugins/customfactory/StringFactoryKey.hpp"
#include "iscore_plugin_ossia_simpleprocess.hpp"

iscore_plugin_ossia_simpleprocess::iscore_plugin_ossia_simpleprocess() :
    QObject {}
{

}

iscore_plugin_ossia_simpleprocess::~iscore_plugin_ossia_simpleprocess()
{

}


std::vector<iscore::FactoryInterfaceBase*> iscore_plugin_ossia_simpleprocess::factories(
        const iscore::ApplicationContext&,
        const iscore::FactoryBaseKey& factoryName) const
{
    if(factoryName == ProcessFactory::staticFactoryKey())
    {
        return {new SimpleProcessFactory};
    }

    return {};
}

