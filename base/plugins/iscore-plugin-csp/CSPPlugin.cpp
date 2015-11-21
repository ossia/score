#include "CSPPlugin.hpp"
#include "CSPApplicationPlugin.hpp"
#include "MoveEventCSPFactory.hpp"
#include "MoveEventCSPFlexFactory.hpp"

iscore_plugin_csp::iscore_plugin_csp() :
    QObject {}
{
}

iscore::GUIApplicationContextPlugin* iscore_plugin_csp::make_applicationplugin(iscore::Presenter* pres)
{
    return new CSPApplicationPlugin{pres};
}

QVector<iscore::FactoryInterface*> iscore_plugin_csp::factories(const QString& factoryName)
{
    if(factoryName == MoveEventFactoryInterface::factoryName())
    {
        return {new MoveEventCSPFactory, new MoveEventCSPFlexFactory};
    }

    return {};
}
