#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

class InspectorWidgetBase;
class QObject;
class QString;
class QWidget;
namespace iscore {
class Document;
}  // namespace iscore

class ScenarioInspectorFactory final : public ProcessInspectorWidgetDelegateFactory
{
    public:
        ScenarioInspectorFactory() = default;

    private:
        ProcessInspectorWidgetDelegate* make(
                const Process&,
                const iscore::DocumentContext&,
                QWidget* parent) const override;
        bool matches(const Process&) const override;

};
