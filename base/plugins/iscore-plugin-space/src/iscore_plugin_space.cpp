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

iscore::GUIApplicationContextPlugin* iscore_plugin_space::make_applicationplugin(iscore::Presenter* pres)
{
    return new SpaceApplicationPlugin{pres};
}

std::vector<std::unique_ptr<iscore::FactoryInterfaceBase>> iscore_plugin_space::factories(const iscore::FactoryBaseKey& factoryName) const
{
    if(factoryName == ProcessFactory::staticFactoryKey())
    {
        return {new SpaceProcessFactory};
    }
    if(factoryName == AreaFactory::staticFactoryKey())
    {
        return {
            new GenericAreaFactory,
                    new CircleAreaFactory
        };
    }

    return {};
}


std::vector<iscore::FactoryFamily> iscore_plugin_space::factoryFamilies()
{
    return {{AreaFactory::staticFactoryKey(),
                    [] (iscore::FactoryInterfaceBase* f)
            {
                if(auto elt = dynamic_cast<AreaFactory*>(f))
                    SingletonAreaFactoryList::instance().inscribe(elt);
            }}};
}
