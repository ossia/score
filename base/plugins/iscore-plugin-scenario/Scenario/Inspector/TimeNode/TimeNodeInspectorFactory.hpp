#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <QList>

class InspectorWidgetBase;
class QObject;
class QString;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

namespace Scenario
{
class TimeNodeInspectorFactory final : public Inspector::InspectorWidgetFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("ff1d130b-caaa-4217-868b-cf09bf75823a")
    public:
        TimeNodeInspectorFactory() :
            InspectorWidgetFactory {}
        {

        }

        Inspector::InspectorWidgetBase* makeWidget(
                const QList<const QObject*>& sourceElements,
                const iscore::DocumentContext& doc,
                QWidget* parent) const override;

        bool matches(const QList<const QObject*>& objects) const override;
};
}
