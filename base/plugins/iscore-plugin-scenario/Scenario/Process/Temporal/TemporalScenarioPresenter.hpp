#pragma once
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>

#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodePresenter.hpp>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp>

#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>

#include <Scenario/Process/Temporal/StateMachines/ScenarioStateMachine.hpp>

#include <Process/LayerPresenter.hpp>
#include <Process/ProcessContext.hpp>
#include <Process/Focus/FocusDispatcher.hpp>
#include <core/document/DocumentContext.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>

#include <Scenario/Application/ScenarioEditionSettings.hpp>
#include <Scenario/Process/Temporal/ScenarioViewInterface.hpp>

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
class SelectionDispatcher;

class TemporalScenarioPresenter final : public LayerPresenter
{
        Q_OBJECT

        friend class Scenario::ToolPalette;
        friend class ScenarioViewInterface;
        friend class ScenarioSelectionManager;

    public:
        TemporalScenarioPresenter(
                iscore::DocumentContext&,
                Scenario::EditionSettings&,
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
        { return m_states; }

        TemporalScenarioView& view() const
        { return *m_view; }
        const ZoomRatio& zoomRatio() const
        { return m_zoomRatio; }

        Scenario::ToolPalette& stateMachine()
        { return m_sm; }
        auto& editionSettings() const
        { return m_editionSettings; }


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

    private:
        void on_focusChanged() override;

        template<typename Map, typename Id>
        void removeElement(Map& map, const Id& id);

        void updateAllElements();
        void eventHasTrigger(const EventPresenter&, bool);

        IdContainer<StatePresenter, StateModel> m_states;
        IdContainer<EventPresenter, EventModel> m_events;
        IdContainer<TimeNodePresenter, TimeNodeModel> m_timeNodes;
        IdContainer<TemporalConstraintPresenter, ConstraintModel> m_constraints;

        ZoomRatio m_zoomRatio {1};

        const TemporalScenarioLayerModel& m_layer;
        TemporalScenarioView* m_view;

        ScenarioViewInterface m_viewInterface;

        Scenario::EditionSettings& m_editionSettings;

        FocusDispatcher m_focusDispatcher;
        LayerContext m_context;
        Scenario::ToolPalette m_sm;
};
