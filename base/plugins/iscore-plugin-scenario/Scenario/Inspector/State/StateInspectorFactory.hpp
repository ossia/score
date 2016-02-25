#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <QList>

class QObject;
class QString;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

namespace Scenario
{
class StateInspectorFactory final : public Inspector::InspectorWidgetFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("780a33ea-408a-4719-b4cc-52a2d8922478")
    public:
        StateInspectorFactory() :
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
