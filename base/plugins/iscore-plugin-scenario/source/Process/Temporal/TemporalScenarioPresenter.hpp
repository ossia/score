#pragma once
#include "Document/State/StatePresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventPresenter.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/TimeNodePresenter.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp"

#include "Document/State/StateModel.hpp"

#include "StateMachines/ScenarioStateMachine.hpp"

#include <ProcessInterface/LayerPresenter.hpp>
#include <ProcessInterface/Focus/FocusDispatcher.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>


namespace iscore
{
    class SerializableCommand;
}
class LayerModel;
class LayerView;

class ConstraintViewModel;
class TemporalConstraintViewModel;
class TemporalConstraintPresenter;

class TemporalScenarioLayerModel;
class TemporalScenarioView;
class TimeNodeModel;
class TimeNodePresenter;
class ConstraintModel;
class ScenarioSelectionManager;
class ScenarioViewInterface;
class SelectionDispatcher;

class TemporalScenarioPresenter : public LayerPresenter
{
        Q_OBJECT

        friend class ScenarioStateMachine;
        friend class ScenarioViewInterface;
        friend class ScenarioSelectionManager;

    public:
        TemporalScenarioPresenter(
                const TemporalScenarioLayerModel& model,
                LayerView* view,
                QObject* parent);
        ~TemporalScenarioPresenter();


        const LayerModel& layerModel() const override;
        const Id<Process>& modelId() const override;

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

        void fillContextMenu(
                QMenu *,
                const QPoint &pos,
                const QPointF &scenepos) const override;

        void handleDrop(
                const QPointF& pos,
                const QMimeData *mime);

    signals:
        void linesExtremityScaled(int, int);

        void keyPressed(int);
        void keyReleased(int);

    public:
        // Model -> view
        void on_stateCreated(const StateModel&);
        void on_stateRemoved(const StateModel&);

        void on_eventCreated(const EventModel&);
        void on_eventRemoved(const EventModel&);

        void on_timeNodeCreated(const TimeNodeModel&);
        void on_timeNodeRemoved(const TimeNodeModel&);

        void on_constraintViewModelCreated(const TemporalConstraintViewModel&);
        void on_constraintViewModelRemoved(const ConstraintViewModel&);

        void on_askUpdate();

    protected:
        IdContainer<StatePresenter, StateModel> m_displayedStates;
        IdContainer<EventPresenter, EventModel> m_events;
        IdContainer<TimeNodePresenter, TimeNodeModel> m_timeNodes;
        IdContainer<TemporalConstraintPresenter, ConstraintModel> m_constraints;

        ZoomRatio m_zoomRatio {1};

        const TemporalScenarioLayerModel& m_layer;
        TemporalScenarioView* m_view;

    private:
        void on_focusChanged() override;

        template<typename Map, typename Id>
        void removeElement(Map& map, const Id& id);


        void updateAllElements();
        void eventHasTrigger(const EventPresenter&, bool);

        ScenarioViewInterface* m_viewInterface{};
        ScenarioStateMachine m_sm;

        FocusDispatcher m_focusDispatcher;
};
