#include "InspectorWidgetList.hpp"
#include "InspectorWidgetBase.hpp"
#include "InspectorWidgetFactoryInterface.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>

#include <QApplication>
#include <core/application/Application.hpp>
#include <core/presenter/Presenter.hpp>
#include <InspectorPlugin/InspectorControl.hpp>

static InspectorWidgetList* inspector_widget_list_instance{};
InspectorWidgetList::InspectorWidgetList(QObject* parent):
    NamedObject{"InspectorWidgetList", parent}
{
    inspector_widget_list_instance = this; // TODO Bleh....
}

InspectorWidgetBase* InspectorWidgetList::makeInspectorWidget(
        const QString& name,
        const QObject& model,
        QWidget* parent)
{
    auto& doc = *iscore::IDocument::documentFromObject(model);

    auto iwl = inspector_widget_list_instance;
    if(iwl)
    {
        for(InspectorWidgetFactory* factory : iwl->m_factories)
        {
            if(factory->matches(name))
            {
                return factory->makeWidget(model, doc, parent);
            }
        }
    }

    // When no factory is found.
    return new InspectorWidgetBase{model, doc, nullptr};
}

void InspectorWidgetList::registerFactory(iscore::FactoryInterfaceBase* e)
{
    m_factories.push_back(
                safe_cast<InspectorWidgetFactory*>(e));
}
