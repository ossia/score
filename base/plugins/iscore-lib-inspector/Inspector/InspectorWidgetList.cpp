#include <iscore/document/DocumentInterface.hpp>

#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include "InspectorWidgetBase.hpp"
#include "InspectorWidgetList.hpp"
#include <iscore/tools/IdentifiedObjectAbstract.hpp>

class QWidget;


namespace Inspector
{
QList<InspectorWidgetBase*> InspectorWidgetList::make(
        const iscore::DocumentContext& doc,
        QList<const IdentifiedObjectAbstract*> models,
        QWidget* parent) const
{
    QList<InspectorWidgetBase*> widgs;
    for(const InspectorWidgetFactory& factory : *this)
    {
        QList<const QObject*> objects;
        objects.reserve(models.size());
        for (auto elt : models)
        {
            objects.push_back(elt);
        }

        if(factory.matches(objects))
        {
            auto widg = factory.makeWidget(objects, doc, parent);
            if(widg)
                widgs.push_back(widg);
        }
    }

    // When no factory is found.
    if(widgs.empty())
        widgs.push_back(new InspectorWidgetBase{*models.first(), doc, nullptr});
    return widgs;
}
}
