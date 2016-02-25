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
class StateInspectorWidget final : public QWidget
{
    public:
        explicit StateInspectorWidget(
                const StateModel& object,
                const iscore::DocumentContext& context,
                QWidget* parent);

        Inspector::InspectorSectionWidget& stateSection()
        { return *m_stateSection; }

        iscore::SelectionDispatcher& selectionDispatcher() const
        { return *m_selectionDispatcher.get(); }

    public slots:
        void splitEvent();

    private:
        void updateDisplayedValues();

        const StateModel& m_model;
        CommandDispatcher<>* m_commandDispatcher{};
        std::unique_ptr<iscore::SelectionDispatcher> m_selectionDispatcher;

        std::list<QWidget*> m_properties;

        Inspector::InspectorSectionWidget* m_stateSection{};
};
}
