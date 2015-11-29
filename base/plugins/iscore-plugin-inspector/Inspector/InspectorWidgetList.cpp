#include <iscore/document/DocumentInterface.hpp>

#include "Inspector/InspectorWidgetFactoryInterface.hpp"
#include "InspectorWidgetBase.hpp"
#include "InspectorWidgetList.hpp"
#include <iscore/tools/IdentifiedObjectAbstract.hpp>

class QWidget;

InspectorWidgetBase* InspectorWidgetList::makeInspectorWidget(
        const QString& name,
        const IdentifiedObjectAbstract& model,
        QWidget* parent) const
{
    auto& doc = *iscore::IDocument::documentFromObject(model);
    for(InspectorWidgetFactory* factory : m_list)
    {
        if(factory->matches(name))
        {
            auto widg = factory->makeWidget(model, doc, parent);
            if(widg)
                return widg;
            break;
        }
    }

    // When no factory is found.
    return new InspectorWidgetBase{model, doc, nullptr};
}
