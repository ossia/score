#pragma once
#include "ProcessInterface/ProcessPresenterInterface.hpp"
#include <iscore/tools/SettableIdentifier.hpp>
#include <ProcessInterface/Focus/FocusDispatcher.hpp>

#include "StateMachines/ScenarioStateMachine.hpp"

namespace iscore
{
    class SerializableCommand;
}
class ProcessViewModelInterface;
class ProcessViewInterface;

class AbstractConstraintViewModel;
class TemporalConstraintViewModel;
class TemporalConstraintPresenter;
class EventPresenter;
class TemporalScenarioViewModel;
class TemporalScenarioView;
class EventModel;
class TimeNodeModel;
class TimeNodePresenter;
class ConstraintModel;
class ScenarioSelectionManager;
class ScenarioViewInterface;
class SelectionDispatcher;

class TemporalScenarioPresenter : public ProcessPresenterInterface
{
        Q_OBJECT

        friend class ScenarioStateMachine;
        friend class ScenarioViewInterface;
        friend class ScenarioSelectionManager;

    public:
        TemporalScenarioPresenter(
                const TemporalScenarioViewModel& model,
                ProcessViewInterface* view,
                QObject* parent);
        ~TemporalScenarioPresenter();


        const id_type<ProcessViewModelInterface>& viewModelId() const override;
        const id_type<ProcessModel>& modelId() const override;

        void setWidth(int width) override;
        void setHeight(int height) override;
        void putToFront() override;
        void putBehind() override;

        void parentGeometryChanged() override;

        void on_zoomRatioChanged(ZoomRatio val) override;

        const std::vector<EventPresenter*>& events() const
        { return m_events; }
        const std::vector<TimeNodePresenter*>& timeNodes() const
        { return m_timeNodes; }
        const std::vector<TemporalConstraintPresenter*>& constraints() const
        { return m_constraints; }

        TemporalScenarioView& view() const
        { return *m_view; }
        const ZoomRatio& zoomRatio() const
        { return m_zoomRatio; }

        ScenarioStateMachine& stateMachine()
        { return m_sm; }

    signals:
        void linesExtremityScaled(int, int);

    public slots:
        // Model -> view
        void on_eventCreated(const id_type<EventModel>& eventId);
        void on_eventDeleted(const id_type<EventModel>& eventId);

        void on_timeNodeCreated(const id_type<TimeNodeModel>& timeNodeId);
        void on_timeNodeDeleted(const id_type<TimeNodeModel>& timeNodeId);

        void on_constraintCreated(const id_type<AbstractConstraintViewModel>& constraintId);
        void on_constraintViewModelRemoved(const id_type<AbstractConstraintViewModel>& constraintId);

        void on_askUpdate();

    protected:
        // TODO faire passer l'abstract et utiliser des free functions de cast
        // TODO boost::multi_index_container
        std::vector<TemporalConstraintPresenter*> m_constraints;
        std::vector<EventPresenter*> m_events;
        std::vector<TimeNodePresenter*> m_timeNodes;

        ZoomRatio m_zoomRatio {1};

        const TemporalScenarioViewModel& m_viewModel;
        TemporalScenarioView* m_view;

    private:
        void on_eventCreated_impl(const EventModel& event_model);
        void on_constraintCreated_impl(const TemporalConstraintViewModel& constraint_view_model);
        void on_timeNodeCreated_impl(const TimeNodeModel& timeNode_model);

        ScenarioViewInterface* m_viewInterface{};
        ScenarioStateMachine m_sm;

        FocusDispatcher m_focusDispatcher;
};
