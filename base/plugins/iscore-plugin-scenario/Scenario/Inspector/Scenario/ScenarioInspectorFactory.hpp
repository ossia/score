#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

class InspectorWidgetBase;
class QObject;
class QString;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

class ScenarioInspectorFactory final : public Process::InspectorWidgetDelegateFactory
{
        ISCORE_CONCRETE_FACTORY_DECL("2d6e103e-6136-43cc-9948-57de2cdf8f31")
    public:
        ScenarioInspectorFactory() = default;

    private:
        Process::InspectorWidgetDelegate* make(
                const Process::ProcessModel&,
                const iscore::DocumentContext&,
                QWidget* parent) const override;
        bool matches(const Process::ProcessModel&) const override;

};
