#include "InspectorWidgetList.hpp"
#include "InspectorWidgetBase.hpp"
#include "InspectorWidgetFactoryInterface.hpp"

#include <QApplication>
InspectorWidgetList::InspectorWidgetList(QObject* parent):
    NamedObject{"InspectorWidgetList", parent}
{
}

InspectorWidgetBase* InspectorWidgetList::makeInspectorWidget(const QString& name,
                                                              const QObject& model,
                                                              QWidget* parent)
{
    auto iwl = qApp->findChild<InspectorWidgetList*>("InspectorWidgetList");

    for(InspectorWidgetFactory* factory : iwl->m_factories)
    {
        if(factory->correspondingObjectsNames().contains(name))
        {
            return factory->makeWidget(model, parent);
        }
    }

    // When no factory is found.
    return new InspectorWidgetBase(model, nullptr);
}

void InspectorWidgetList::registerFactory(iscore::FactoryInterface* e)
{
    m_factories.push_back(
                static_cast<InspectorWidgetFactory*>(e));
}
