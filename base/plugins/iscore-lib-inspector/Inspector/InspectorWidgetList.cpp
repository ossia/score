#include <iscore/document/DocumentInterface.hpp>

#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include "InspectorWidgetBase.hpp"
#include "InspectorWidgetList.hpp"
#include <iscore/tools/IdentifiedObjectAbstract.hpp>

class QWidget;


namespace Inspector
{
InspectorWidgetBase* InspectorWidgetList::make(
        const iscore::DocumentContext& doc,
        const IdentifiedObjectAbstract& model,
        QWidget* parent) const
{
    for(const InspectorWidgetFactory& factory : *this)
    {
        if(factory.matches(model))
        {
            auto widg = factory.makeWidget(model, doc, parent);
            if(widg)
                return widg;
            break;
        }
    }

    // When no factory is found.
    return new InspectorWidgetBase{model, doc, nullptr};
}
}
