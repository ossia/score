#include "InspectorWidgetList.hpp"
#include "InspectorWidgetBase.hpp"
#include "InspectorWidgetFactoryInterface.hpp"

#include <QApplication>
InspectorWidgetList::InspectorWidgetList(QObject* parent):
    NamedObject{"InspectorWidgetList", parent}
{
}

InspectorWidgetBase* InspectorWidgetList::makeInspectorWidget(QString name, QObject* object)
{
    auto iwl = qApp->findChild<InspectorWidgetList*>("InspectorWidgetList");

    for(auto& factory : iwl->m_factories)
    {
        if(factory->correspondingObjectsNames().contains(name))
        {
            return factory->makeWidget(object);
        }
    }

    // When no factory is found.
    return new InspectorWidgetBase(object);
}

void InspectorWidgetList::registerFactory(iscore::FactoryInterface* e)
{
    m_factories.push_back(
                static_cast<InspectorWidgetFactoryInterface*>(e));
}
