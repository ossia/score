#pragma once
// TODO is this necessary ?
#include "Document/State/StatePresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventPresenter.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/TimeNodePresenter.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp"

#include "Document/State/DisplayedStateModel.hpp"

#include "StateMachines/ScenarioStateMachine.hpp"

#include <ProcessInterface/ProcessPresenter.hpp>
#include <ProcessInterface/Focus/FocusDispatcher.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>


namespace iscore
{
    class SerializableCommand;
}
class LayerModel;
class Layer;

class ConstraintViewModel;
class TemporalConstraintViewModel;
class TemporalConstraintPresenter;

class TemporalScenarioLayer;
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
                const TemporalScenarioLayer& model,
                Layer* view,
                QObject* parent);
        ~TemporalScenarioPresenter();


        const LayerModel& viewModel() const override;
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
        const auto& states() const
        { return m_displayedStates; }

        TemporalScenarioView& view() const
        { return *m_view; }
        const ZoomRatio& zoomRatio() const
        { return m_zoomRatio; }

        ScenarioStateMachine& stateMachine()
        { return m_sm; }


        void handleDrop(const QPointF& pos, const QMimeData *mime);


    signals:
        void linesExtremityScaled(int, int);

        void keyPressed(int);
        void keyReleased(int);

        void contextMenuAsked(const QPoint&);

    public slots:
        // Model -> view
        void on_stateCreated(const id_type<StateModel>& eventId);
        void on_stateRemoved(const id_type<StateModel>& eventId);

        void on_eventCreated(const id_type<EventModel>& eventId);
        void on_eventRemoved(const id_type<EventModel>& eventId);

        void on_timeNodeCreated(const id_type<TimeNodeModel>& timeNodeId);
        void on_timeNodeRemoved(const id_type<TimeNodeModel>& timeNodeId);

        void on_constraintViewModelCreated(const id_type<ConstraintViewModel>& constraintId);
        void on_constraintViewModelRemoved(const id_type<ConstraintViewModel>& constraintId);

        void on_askUpdate();

    protected:
        // TODO faire passer l'abstract et utiliser des free functions de cast?
        IdContainer<StatePresenter, StateModel> m_displayedStates;
        IdContainer<EventPresenter, EventModel> m_events;
        IdContainer<TimeNodePresenter, TimeNodeModel> m_timeNodes;
        IdContainer<TemporalConstraintPresenter, ConstraintModel> m_constraints;

        ZoomRatio m_zoomRatio {1};

        const TemporalScenarioLayer& m_layer;
        TemporalScenarioView* m_view;

    private:
        template<typename Map, typename Id>
        void removeElement(Map& map, const Id& id);

        void on_stateCreated_impl(const StateModel& state);
        void on_eventCreated_impl(const EventModel& event_model);
        void on_timeNodeCreated_impl(const TimeNodeModel& timeNode_model);
        void on_constraintCreated_impl(const TemporalConstraintViewModel& constraint_view_model);

        void updateAllElements();
        void eventHasTrigger(const EventPresenter&, bool);

        ScenarioViewInterface* m_viewInterface{};
        ScenarioStateMachine m_sm;

        FocusDispatcher m_focusDispatcher;
};
