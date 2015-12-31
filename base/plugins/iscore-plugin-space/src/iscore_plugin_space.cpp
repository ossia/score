#include "iscore_plugin_space.hpp"

#include "SpaceProcessFactory.hpp"
#include "SpaceApplicationPlugin.hpp"
#include "Area/AreaFactory.hpp"
#include "Area/SingletonAreaFactoryList.hpp"

#include "Area/Circle/CircleAreaFactory.hpp"
#include "Area/Pointer/PointerAreaFactory.hpp"
#include "Area/Generic/GenericAreaFactory.hpp"
#include <src/SpaceProcess.hpp>
#include <iscore/plugins/customfactory/FactoryFamily.hpp>
#include <iscore/plugins/customfactory/FactorySetup.hpp>

iscore_plugin_space::iscore_plugin_space() :
    QObject {}
{
}

iscore_plugin_space::~iscore_plugin_space()
{

}

iscore::GUIApplicationContextPlugin* iscore_plugin_space::make_applicationPlugin(
        const iscore::ApplicationContext& pres)
{
    return new SpaceApplicationPlugin{pres};
}

std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> iscore_plugin_space::factories(
        const iscore::ApplicationContext& ctx,
        const iscore::FactoryBaseKey& key) const
{

    return instantiate_factories<
            iscore::ApplicationContext,
    TL<
        FW<Process::ProcessFactory,
            Space::ProcessFactory>,
        FW<AreaFactory,
            GenericAreaFactory,
            CircleAreaFactory,
            PointerAreaFactory>,
        FW<OSSIA::LocalTree::ProcessComponentFactory,
            Space::ProcessLocalTreeFactory>
            >
     >(ctx, key);
}


std::vector<std::unique_ptr<iscore::FactoryListInterface>> iscore_plugin_space::factoryFamilies()
{
    return make_ptr_vector<iscore::FactoryListInterface,
            SingletonAreaFactoryList>();
}
