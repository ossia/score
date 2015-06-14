#pragma once
#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventPresenter.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/TimeNodePresenter.hpp"


#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp"

#include "StateMachines/ScenarioStateMachine.hpp"

#include <ProcessInterface/ProcessPresenter.hpp>
#include <ProcessInterface/Focus/FocusDispatcher.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>


namespace iscore
{
    class SerializableCommand;
}
class ProcessViewModel;
class ProcessView;

class AbstractConstraintViewModel;
class TemporalConstraintViewModel;
class TemporalConstraintPresenter;

class TemporalScenarioViewModel;
class TemporalScenarioView;
class TimeNodeModel;
class TimeNodePresenter;
class ConstraintModel;
class ScenarioSelectionManager;
class ScenarioViewInterface;
class SelectionDispatcher;

class TemporalScenarioPresenter : public ProcessPresenter
{
        Q_OBJECT

        friend class ScenarioStateMachine;
        friend class ScenarioViewInterface;
        friend class ScenarioSelectionManager;

    public:
        TemporalScenarioPresenter(
                const TemporalScenarioViewModel& model,
                ProcessView* view,
                QObject* parent);
        ~TemporalScenarioPresenter();


        const ProcessViewModel& viewModel() const override;
        const id_type<ProcessModel>& modelId() const override;

        void setWidth(int width) override;
        void setHeight(int height) override;
        void putToFront() override;
        void putBehind() override;

        void parentGeometryChanged() override;

        void on_zoomRatioChanged(ZoomRatio val) override;

        const auto& events() const
        { return m_events; }
        const auto& timeNodes() const
        { return m_timeNodes; }
        const auto& constraints() const
        { return m_constraints; }

        TemporalScenarioView& view() const
        { return *m_view; }
        const ZoomRatio& zoomRatio() const
        { return m_zoomRatio; }

        ScenarioStateMachine& stateMachine()
        { return m_sm; }

    signals:
        void linesExtremityScaled(int, int);
        void shiftPressed();
        void shiftReleased();
        void contextMenuAsked(const QPoint&);

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
        // TODO faire passer l'abstract et utiliser des free functions de cast?
        IdContainer<TemporalConstraintPresenter, ConstraintModel> m_constraints;
        IdContainer<EventPresenter, EventModel> m_events;
        IdContainer<TimeNodePresenter, TimeNodeModel> m_timeNodes;

        ZoomRatio m_zoomRatio {1};

        const TemporalScenarioViewModel& m_viewModel;
        TemporalScenarioView* m_view;

    private:
        void on_eventCreated_impl(const EventModel& event_model);
        void on_constraintCreated_impl(const TemporalConstraintViewModel& constraint_view_model);
        void on_timeNodeCreated_impl(const TimeNodeModel& timeNode_model);

        void eventHasTrigger(const EventPresenter&, bool);

        ScenarioViewInterface* m_viewInterface{};
        ScenarioStateMachine m_sm;

        FocusDispatcher m_focusDispatcher;
};
