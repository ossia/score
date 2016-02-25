#pragma once
#include <Inspector/InspectorWidgetFactoryInterface.hpp>
#include <QList>

namespace Scenario
{
class ScenarioInspectorWidgetFactoryWrapper final : public Inspector::InspectorWidgetFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("066fffc1-c82c-4ffd-ad7c-55a65bfa067f")
    public:
        ScenarioInspectorWidgetFactoryWrapper() :
            InspectorWidgetFactory {}
        {

        }

        Inspector::InspectorWidgetBase*makeWidget(
        const QList<const QObject*>& sourceElements,
            const iscore::DocumentContext& doc,
            QWidget* parent) const override;

        bool matches(const QList<const QObject*>& objects) const override;
};

}
