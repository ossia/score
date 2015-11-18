#include "InspectorWidgetList.hpp"
#include "InspectorWidgetBase.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>

#include <QApplication>
#include <core/application/Application.hpp>
#include <core/presenter/Presenter.hpp>

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
            return factory->makeWidget(model, doc, parent);
        }
    }

    // When no factory is found.
    return new InspectorWidgetBase{model, doc, nullptr};
}
