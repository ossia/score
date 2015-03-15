#include "StatePlugin.hpp"
#include "Inspector/StateInspector.hpp"


StatePlugin::StatePlugin()
{

}

QVector<iscore::FactoryInterface*> StatePlugin::factories_make(QString factoryName)
{

    return {};
}
