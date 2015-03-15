#include "StateInspectorFactory.hpp"
#include "StateInspector.hpp"
QList<QString> StateInspectorFactory::correspondingObjectsNames() const
{
    return {"State"};
}

InspectorWidgetBase* StateInspectorFactory::makeWidget(QObject* sourceElement)
{

}
