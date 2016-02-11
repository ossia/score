#pragma once

#include <Inspector/InspectorWidgetBase.hpp>
#include <list>

namespace Inspector
{
class InspectorSectionWidget;
}
class QWidget;
namespace iscore {
struct DocumentContext;
}  // namespace iscore

namespace Scenario
{
class StateModel;
class StateInspectorWidget final : public Inspector::InspectorWidgetBase
{
    public:
        explicit StateInspectorWidget(
                const StateModel& object,
                const iscore::DocumentContext& context,
                QWidget* parent);

        Inspector::InspectorSectionWidget& stateSection()
        { return *m_stateSection; }

    public slots:
        void splitEvent();

    private:
        QString tabName() override;

        void updateDisplayedValues();

        std::list<QWidget*> m_properties;

        const StateModel& m_model;

        Inspector::InspectorSectionWidget* m_stateSection{};
};
}
