#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <list>

class InspectorSectionWidget;
class QWidget;
namespace iscore {
struct DocumentContext;
}  // namespace iscore

namespace Scenario
{
class StateModel;
class StateInspectorWidget final : public InspectorWidgetBase
{
    public:
        explicit StateInspectorWidget(
                const StateModel& object,
                const iscore::DocumentContext& context,
                QWidget* parent);

    private:
        void updateDisplayedValues();
        void splitEvent();

        std::list<QWidget*> m_properties;

        const StateModel& m_model;

        InspectorSectionWidget* m_stateSection{};
};
}
