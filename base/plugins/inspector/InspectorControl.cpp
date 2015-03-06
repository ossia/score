#include "InspectorControl.hpp"
#include "InspectorInterface/InspectorWidgetBase.hpp"
#include <QApplication>

InspectorWidgetBase* InspectorControl::makeInspectorWidget(QObject* object)
{
    // TODO make the same thing than for ProcessList here, with an InspectorInterface.
    auto pmgr = qApp->findChild<InspectorControl*> ("InspectorControl");

    auto factories = pmgr->factories();

    for(auto factory : factories)
    {
        if(factory->correspondingObjectsNames().contains(object->objectName()))
        {
            return factory->makeWidget(object);  // TODO multiple items.
        }
    }

    // When no factory is found.
    return new InspectorWidgetBase(object);
}
