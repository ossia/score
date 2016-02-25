#pragma once
#include <iscore/plugins/customfactory/UuidKey.hpp>
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
namespace Process
{
class StateProcessFactory;
class StateProcess;
}
namespace Scenario
{
class StateModel;
class StateInspectorWidget final :
        public QWidget,
        public Nano::Observer
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
        void on_stateProcessCreated(const Process::StateProcess&);
        void on_stateProcessRemoved(const Process::StateProcess&);
        void createStateProcess(const UuidKey<Process::StateProcessFactory>&);
        Inspector::InspectorSectionWidget* displayStateProcess(
                const Process::StateProcess &process);
        void updateDisplayedValues();

        const StateModel& m_model;
        const iscore::DocumentContext& m_context;
        CommandDispatcher<>* m_commandDispatcher{};
        std::unique_ptr<iscore::SelectionDispatcher> m_selectionDispatcher;

        std::list<QWidget*> m_properties;

        Inspector::InspectorSectionWidget* m_stateSection{};
};
}
