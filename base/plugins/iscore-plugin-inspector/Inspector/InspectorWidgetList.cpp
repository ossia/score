#include "InspectorWidgetList.hpp"
#include "InspectorWidgetBase.hpp"
#include "InspectorWidgetFactoryInterface.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>

#include <QApplication>
InspectorWidgetList::InspectorWidgetList(QObject* parent):
    NamedObject{"InspectorWidgetList", parent}
{
}

InspectorWidgetBase* InspectorWidgetList::makeInspectorWidget(
        const QString& name,
        const QObject& model,
        QWidget* parent)
{
    auto iwl = qApp->findChild<InspectorWidgetList*>("InspectorWidgetList");
    auto& doc = *iscore::IDocument::documentFromObject(model);

    for(InspectorWidgetFactory* factory : iwl->m_factories)
    {
        if(factory->correspondingObjectsNames().contains(name))
        {
            return factory->makeWidget(model, doc, parent);
        }
    }

    // When no factory is found.
    return new InspectorWidgetBase{model, doc, nullptr};
}

void InspectorWidgetList::registerFactory(iscore::FactoryInterface* e)
{
    m_factories.push_back(
                static_cast<InspectorWidgetFactory*>(e));
}
