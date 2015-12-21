#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

class InspectorWidgetBase;
class QObject;
class QString;
class QWidget;
namespace iscore {
struct DocumentContext;
}  // namespace iscore


class AutomationInspectorFactory final : public ProcessInspectorWidgetDelegateFactory
{
    public:
        AutomationInspectorFactory() = default;

    private:
        ProcessInspectorWidgetDelegate* make(
                const Process&,
                const iscore::DocumentContext& doc,
                QWidget* parent) const override;
        bool matches(const Process&) const override;

};
