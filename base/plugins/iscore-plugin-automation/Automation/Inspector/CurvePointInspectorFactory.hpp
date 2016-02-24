#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <QList>

class QObject;
class QString;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

namespace Automation
{
class PointInspectorFactory final :
        public Inspector::InspectorWidgetFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("c2fc4c5b-641f-41e3-8734-5caf77b27de8")
    public:
    PointInspectorFactory() :
        InspectorWidgetFactory {}
    {

    }

    Inspector::InspectorWidgetBase* makeWidget(
        QList<const QObject*> sourceElements,
        const iscore::DocumentContext& doc,
        QWidget* parent) const override;

    bool matches(QList<const QObject*> objects) const override;
};
}
