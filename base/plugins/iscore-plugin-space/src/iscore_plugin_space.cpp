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

std::vector<iscore::FactoryInterfaceBase*> iscore_plugin_space::factories(const std::string& factoryName) const
{
    if(factoryName == ProcessFactory::staticFactoryName())
    {
        return {new SpaceProcessFactory};
    }
    if(factoryName == AreaFactory::staticFactoryName())
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
    return {{AreaFactory::staticFactoryName(),
                    [] (iscore::FactoryInterfaceBase* f)
            {
                if(auto elt = dynamic_cast<AreaFactory*>(f))
                    SingletonAreaFactoryList::instance().inscribe(elt);
            }}};
}
