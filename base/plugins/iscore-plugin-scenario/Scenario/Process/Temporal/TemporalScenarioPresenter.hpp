#pragma once
#include <Process/Focus/FocusDispatcher.hpp>
#include <Process/LayerPresenter.hpp>
#include <Process/ProcessContext.hpp>
#include <Scenario/Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp>
#include <Scenario/Palette/ScenarioPalette.hpp>
#include <Scenario/Process/Temporal/ScenarioViewInterface.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <QObject>
#include <QPoint>

#include <Process/ZoomHelper.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Event/EventPresenter.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/State/StatePresenter.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodePresenter.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockPresenter.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

#include <iscore/command/Dispatchers/OngoingCommandDispatcher.hpp>

namespace Process { class ProcessModel; }
class QEvent;
class QMenu;
class QMimeData;
namespace iscore {
struct DocumentContext;
}  // namespace iscore

namespace iscore
{
}
namespace Process {
class LayerModel;
class LayerView;
}

namespace Scenario
{

class EditionSettings;
class ConstraintViewModel;
class TemporalConstraintViewModel;
class TemporalScenarioLayerModel;
class TemporalScenarioView;

class ISCORE_PLUGIN_SCENARIO_EXPORT TemporalScenarioPresenter final :
        public Process::LayerPresenter
{
        Q_OBJECT

        friend class Scenario::ToolPalette;
        friend class ScenarioViewInterface;
        friend class ScenarioSelectionManager;

    public:
        TemporalScenarioPresenter(
                const iscore::DocumentContext&,
                Scenario::EditionSettings&,
                const TemporalScenarioLayerModel& model,
                Process::LayerView* view,
                QObject* parent);
        ~TemporalScenarioPresenter();


        const Process::LayerModel& layerModel() const override;
        const Id<Process::ProcessModel>& modelId() const override;

        void setWidth(qreal width) override;
        void setHeight(qreal height) override;
        void putToFront() override;
        void putBehind() override;

        void parentGeometryChanged() override;

        void on_zoomRatioChanged(ZoomRatio val) override;

        const auto& event(const Id<EventModel>& id) const
        { return m_events.at(id); }
        const auto& timeNode(const Id<TimeNodeModel>& id) const
        { return m_timeNodes.at(id); }
        const auto& constraint(const Id<ConstraintModel>& id) const
        { return m_constraints.at(id); }
        const auto& state(const Id<StateModel>& id) const
        { return m_states.at(id); }
        const auto& comment(const Id<CommentBlockModel>& id) const
        { return m_comments.at(id); }
        const auto& events() const
        { return m_events; }
        const auto& timeNodes() const
        { return m_timeNodes; }
        const auto& constraints() const
        { return m_constraints; }
        const auto& states() const
        { return m_states; }
        const auto& comments() const
        { return m_comments; }

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

        bool event(QEvent* e) override
        {
            return QObject::event(e);
        }
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

        void on_commentBlockCreated(const CommentBlockModel&);
        void on_commentBlockRemoved(const CommentBlockModel&);

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
        IdContainer<CommentBlockPresenter, CommentBlockModel> m_comments;

        ZoomRatio m_zoomRatio {1};
        double m_graphicalScale{1.};

        const TemporalScenarioLayerModel& m_layer;
        TemporalScenarioView* m_view;

        ScenarioViewInterface m_viewInterface;

        Scenario::EditionSettings& m_editionSettings;

        OngoingCommandDispatcher m_ongoingDispatcher;

        FocusDispatcher m_focusDispatcher;
        iscore::SelectionDispatcher m_selectionDispatcher;
        LayerContext m_context;
        Scenario::ToolPalette m_sm;

        Scenario::Tool m_lastTool{Scenario::Tool::Select};

};
}
