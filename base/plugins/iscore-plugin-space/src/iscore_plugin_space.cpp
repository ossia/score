#include "iscore_plugin_space.hpp"

#include "SpaceProcessFactory.hpp"
#include "SpaceApplicationPlugin.hpp"
#include "Area/AreaFactory.hpp"
#include "Area/SingletonAreaFactoryList.hpp"

#include "Area/Circle/CircleAreaFactory.hpp"
#include "Area/Generic/GenericAreaFactory.hpp"
#include <iscore/plugins/customfactory/FactoryFamily.hpp>

iscore_plugin_space::iscore_plugin_space() :
    QObject {}
{
}

iscore::GUIApplicationContextPlugin* iscore_plugin_space::make_applicationPlugin(
        const iscore::ApplicationContext& pres)
{
    return new SpaceApplicationPlugin{pres};
}

std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> iscore_plugin_space::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::FactoryBaseKey& factoryName) const
{
    if(factoryName == ProcessFactory::staticFactoryKey())
    {
        return make_ptr_vector<iscore::FactoryInterfaceBase,
                Space::ProcessFactory>();
    }
    if(factoryName == AreaFactory::staticFactoryKey())
    {
        return make_ptr_vector<iscore::FactoryInterfaceBase,
                GenericAreaFactory,
                CircleAreaFactory>();
    }

    return {};
}


std::vector<std::unique_ptr<iscore::FactoryListInterface>> iscore_plugin_space::factoryFamilies()
{
    return make_ptr_vector<iscore::FactoryListInterface,
            SingletonAreaFactoryList>();
}
