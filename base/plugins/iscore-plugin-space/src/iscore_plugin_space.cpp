#include "iscore_plugin_space.hpp"

#include "SpaceProcessFactory.hpp"
#include "SpaceApplicationPlugin.hpp"
#include "Area/AreaFactory.hpp"
#include "Area/SingletonAreaFactoryList.hpp"

#include "Area/Circle/CircleAreaFactory.hpp"
#include "Area/Pointer/PointerAreaFactory.hpp"
#include "Area/Generic/GenericAreaFactory.hpp"
#include <src/SpaceProcess.hpp>
#include <src/LocalTree/AreaComponent.hpp>
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
        FW<Space::AreaFactory,
            Space::GenericAreaFactory,
            Space::CircleAreaFactory,
            Space::PointerAreaFactory>,
        FW<Ossia::LocalTree::ProcessComponentFactory,
            Space::LocalTree::ProcessLocalTreeFactory>,
        FW<Space::LocalTree::AreaComponentFactory,
            Space::LocalTree::GenericAreaComponentFactory // Shall be last in the vector so must be first here, because of the recursion order of C++ templates in instantiate_factories
            >
            // , FW<Space::LocalTree::ComputationComponentFactory>
      >
     >(ctx, key);
}


std::vector<std::unique_ptr<iscore::FactoryListInterface>> iscore_plugin_space::factoryFamilies()
{
    return make_ptr_vector<iscore::FactoryListInterface,
            Space::SingletonAreaFactoryList,
            Space::LocalTree::AreaComponentFactoryList,
            Space::LocalTree::ComputationComponentFactoryList
            >();
}
