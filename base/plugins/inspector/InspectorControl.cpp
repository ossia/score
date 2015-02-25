#include "InspectorControl.hpp"
#include "InspectorInterface/InspectorWidgetBase.hpp"
#include <QApplication>

InspectorWidgetBase*InspectorControl::makeInspectorWidget(QObject* object)
{
    auto pmgr = qApp->findChild<InspectorControl*>("InspectorControl");

    auto factories = pmgr->factories();
    for(auto factory : factories)
    {
		if(factory->correspondingObjectsNames().contains(object->objectName()))
        {
            return factory->makeWidget(object); // TODO multiple items.
        }
    }

    // When no factory is found.
    return new InspectorWidgetBase(object);
}
