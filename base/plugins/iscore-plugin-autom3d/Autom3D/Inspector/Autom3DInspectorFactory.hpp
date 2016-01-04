#pragma once
#include <Process/Inspector/ProcessInspectorWidgetDelegateFactory.hpp>

class InspectorWidgetBase;
class QObject;
class QString;
class QWidget;
namespace iscore {
struct DocumentContext;
}  // namespace iscore


namespace Autom3D
{
class InspectorFactory final : public ProcessInspectorWidgetDelegateFactory
{
    public:
        InspectorFactory() = default;

    private:
        ProcessInspectorWidgetDelegate* make(
                const Process::ProcessModel&,
                const iscore::DocumentContext& doc,
                QWidget* parent) const override;
        bool matches(const Process::ProcessModel&) const override;

};
}
