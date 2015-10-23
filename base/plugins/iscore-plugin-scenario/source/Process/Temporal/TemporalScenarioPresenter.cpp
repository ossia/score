#include "TemporalScenarioPresenter.hpp"

#include "source/Process/ScenarioModel.hpp"
#include "source/Process/Temporal/TemporalScenarioLayerModel.hpp"
#include "source/Process/Temporal/TemporalScenarioView.hpp"

#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintPresenter.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Rack/RackPresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventPresenter.hpp"
#include "Document/Event/EventView.hpp"

#include "Document/TimeNode/TimeNodeModel.hpp"
#include "Document/TimeNode/TimeNodeView.hpp"
#include "Document/TimeNode/TimeNodePresenter.hpp"

#include <Document/TimeNode/Trigger/TriggerModel.hpp>
#include <Document/TimeNode/Trigger/TriggerView.hpp>

#include "Document/State/StateModel.hpp"

#include "ScenarioViewInterface.hpp"
#include <Control/ScenarioControl.hpp>

#include <QGraphicsScene>

#include <State/StateMimeTypes.hpp>
#include <State/MessageListSerialization.hpp>
#include <QMimeData>
#include <iscore/widgets/GraphicsItem.hpp>
#include <QJsonDocument>
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <core/document/Document.hpp>
#include <Commands/State/UpdateState.hpp>
#include "Commands/Scenario/Creations/CreateTimeNode_Event_State.hpp"
#include "Commands/Scenario/Creations/CreateStateMacro.hpp"
#include <Document/BaseElement/BaseElementModel.hpp>

TemporalScenarioPresenter::TemporalScenarioPresenter(
        const TemporalScenarioLayerModel& process_view_model,
        LayerView* view,
        QObject* parent) :
    LayerPresenter {"TemporalScenarioPresenter", parent},
    m_layer {process_view_model},
    m_view {static_cast<TemporalScenarioView*>(view)},
    m_viewInterface{new ScenarioViewInterface{this}},
    m_sm{*iscore::IDocument::documentFromObject(m_layer.processModel()), *this}, // OPTIMIZEME by passing document in parent
    m_focusDispatcher{*iscore::IDocument::documentFromObject(m_layer.processModel())}
{
    const ScenarioModel& scenario = model(m_layer);
    /////// Setup of existing data
    // For each constraint & event, display' em
    for(const auto& state_model : scenario.states)
    {
        on_stateCreated(state_model);
    }

    for(const auto& event_model : scenario.events)
    {
        on_eventCreated(event_model);
    }

    for(const auto& tn_model : scenario.timeNodes)
    {
        on_timeNodeCreated(tn_model);
    }

    for(const auto& constraint_view_model : constraintsViewModels(m_layer))
    {
        on_constraintViewModelCreated(*constraint_view_model);
    }


    /////// Connections
    con(m_layer, &TemporalScenarioLayerModel::stateCreated,
        this, &TemporalScenarioPresenter::on_stateCreated);
    con(m_layer, &TemporalScenarioLayerModel::stateRemoved,
        this, &TemporalScenarioPresenter::on_stateRemoved);

    con(m_layer, &TemporalScenarioLayerModel::eventCreated,
        this, &TemporalScenarioPresenter::on_eventCreated);
    con(m_layer, &TemporalScenarioLayerModel::eventRemoved,
        this, &TemporalScenarioPresenter::on_eventRemoved);

    con(m_layer, &TemporalScenarioLayerModel::timeNodeCreated,
        this, &TemporalScenarioPresenter::on_timeNodeCreated);
    con(m_layer, &TemporalScenarioLayerModel::timeNodeRemoved,
        this, &TemporalScenarioPresenter::on_timeNodeRemoved);

    con(m_layer, &TemporalScenarioLayerModel::constraintViewModelCreated,
        this, &TemporalScenarioPresenter::on_constraintViewModelCreated);
    con(m_layer, &TemporalScenarioLayerModel::constraintViewModelRemoved,
        this, &TemporalScenarioPresenter::on_constraintViewModelRemoved);

    connect(m_view, &TemporalScenarioView::scenarioPressed,
            this, [&] (const QPointF&)
    {
        m_focusDispatcher.focus(this);
    });

    connect(m_view, &TemporalScenarioView::keyPressed,
            this,   &TemporalScenarioPresenter::keyPressed);
    connect(m_view, &TemporalScenarioView::keyReleased,
            this,   &TemporalScenarioPresenter::keyReleased);

    connect(m_view, &TemporalScenarioView::askContextMenu,
            this,   &TemporalScenarioPresenter::contextMenuRequested);
    connect(m_view, &TemporalScenarioView::dropReceived,
            this,   &TemporalScenarioPresenter::handleDrop);

    con(model(m_layer), &ScenarioModel::locked,
            m_view,             &TemporalScenarioView::lock);
    con(model(m_layer), &ScenarioModel::unlocked,
            m_view,             &TemporalScenarioView::unlock);

    m_sm.start();
}

TemporalScenarioPresenter::~TemporalScenarioPresenter()
{
    delete m_viewInterface;

    deleteGraphicsObject(m_view);
}

const LayerModel& TemporalScenarioPresenter::layerModel() const
{
    return m_layer;
}

const Id<Process>& TemporalScenarioPresenter::modelId() const
{
    return m_layer.processModel().id();
}

void TemporalScenarioPresenter::setWidth(int width)
{
    m_view->setWidth(width);
}

void TemporalScenarioPresenter::setHeight(int height)
{
    m_view->setHeight(height);
}

void TemporalScenarioPresenter::putToFront()
{
    m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, false);
    m_view->setOpacity(1);
}

void TemporalScenarioPresenter::putBehind()
{
    m_view->setFlag(QGraphicsItem::ItemStacksBehindParent, true);
    m_view->setOpacity(0.1);
}

void TemporalScenarioPresenter::parentGeometryChanged()
{
    updateAllElements();
    m_view->update();
}

void TemporalScenarioPresenter::on_zoomRatioChanged(ZoomRatio val)
{
    m_zoomRatio = val;

    for(auto& constraint : m_constraints)
    {
        constraint.on_zoomRatioChanged(m_zoomRatio);
    }
}

void TemporalScenarioPresenter::fillContextMenu(
        QMenu* menu,
        const QPoint& pos,
        const QPointF& scenepos) const
{
    ScenarioControl::instance()->contextMenuDispatcher.createScenarioContextMenu(*menu, pos, scenepos, *this);
}

template<typename Map, typename Id>
void TemporalScenarioPresenter::removeElement(
        Map& map,
        const Id& id)
{
    auto it = map.find(id);
    if(it != map.end())
    {
        delete *it;
        map.erase(it);
    }

    m_view->update();
}

void TemporalScenarioPresenter::on_stateRemoved(
        const StateModel& state)
{
    removeElement(m_displayedStates.get(), state.id());
}


void TemporalScenarioPresenter::on_eventRemoved(
        const EventModel& event)
{
    removeElement(m_events.get(), event.id());
}

void TemporalScenarioPresenter::on_timeNodeRemoved(
        const TimeNodeModel& timeNode)
{
    removeElement(m_timeNodes.get(), timeNode.id());
}

void TemporalScenarioPresenter::on_constraintViewModelRemoved(
        const ConstraintViewModel& cvm)
{
    // Don't put a const auto& here, else deletion will crash.
    for(auto& pres : m_constraints)
    {
        // OPTIMIZEME add an index in the map on viewmodel id ?
        if(::viewModel(pres).id() == cvm.id())
        {
            auto cid = pres.id();
            auto it = m_constraints.find(cid);
            if(it != m_constraints.end())
            {
                m_constraints.remove(cid);
                delete &pres;
            }

            m_view->update();
            break;
        }
    }
}

/////////////////////////////////////////////////////////////////////
// USER INTERACTIONS
void TemporalScenarioPresenter::on_askUpdate()
{
    m_view->update();
}

void TemporalScenarioPresenter::on_focusChanged()
{
    m_view->setFocus();
}

/////////////////////////////////////////////////////////////////////
// ELEMENTS CREATED
void TemporalScenarioPresenter::on_eventCreated(const EventModel& event_model)
{
    auto ev_pres = new EventPresenter {event_model,
                           m_view,
                           this};
    m_events.insert(ev_pres);

    m_viewInterface->on_eventMoved(*ev_pres);

    con(event_model, &EventModel::extentChanged,
            this, [=] (const VerticalExtent&) { m_viewInterface->on_eventMoved(*ev_pres); });
    con(event_model, &EventModel::dateChanged,
            this, [=] (const TimeValue&) { m_viewInterface->on_eventMoved(*ev_pres); });

    connect(ev_pres, &EventPresenter::eventHoverEnter,
            this, [=] () { m_viewInterface->on_hoverOnEvent(ev_pres->id(), true); });
    connect(ev_pres, &EventPresenter::eventHoverLeave,
            this, [=] () { m_viewInterface->on_hoverOnEvent(ev_pres->id(), false); });

    // For the state machine
    connect(ev_pres, &EventPresenter::pressed, m_view, &TemporalScenarioView::scenarioPressed);
    connect(ev_pres, &EventPresenter::moved, m_view, &TemporalScenarioView::scenarioMoved);
    connect(ev_pres, &EventPresenter::released, m_view, &TemporalScenarioView::scenarioReleased);
}

void TemporalScenarioPresenter::on_timeNodeCreated(const TimeNodeModel& timeNode_model)
{
    auto tn_pres = new TimeNodePresenter {timeNode_model, m_view, this};
    m_timeNodes.insert(tn_pres);

    m_viewInterface->on_timeNodeMoved(*tn_pres);

    con(timeNode_model, &TimeNodeModel::extentChanged,
            this, [=] (const VerticalExtent&) { m_viewInterface->on_timeNodeMoved(*tn_pres); });
    con(timeNode_model, &TimeNodeModel::dateChanged,
            this, [=] (const TimeValue&) { m_viewInterface->on_timeNodeMoved(*tn_pres); });

    // For the state machine
    connect(tn_pres, &TimeNodePresenter::pressed, m_view, &TemporalScenarioView::scenarioPressed);
    connect(tn_pres, &TimeNodePresenter::moved, m_view, &TemporalScenarioView::scenarioMoved);
    connect(tn_pres, &TimeNodePresenter::released, m_view, &TemporalScenarioView::scenarioReleased);
}

void TemporalScenarioPresenter::on_stateCreated(const StateModel &state)
{
    auto st_pres = new StatePresenter{state, m_view, this};
    m_displayedStates.insert(st_pres);

    m_viewInterface->on_stateMoved(*st_pres);

    con(state, &StateModel::heightPercentageChanged,
            this, [=] () { m_viewInterface->on_stateMoved(*st_pres); });

    // For the state machine
    connect(st_pres, &StatePresenter::pressed, m_view, &TemporalScenarioView::scenarioPressed);
    connect(st_pres, &StatePresenter::moved, m_view, &TemporalScenarioView::scenarioMoved);
    connect(st_pres, &StatePresenter::released, m_view, &TemporalScenarioView::scenarioReleased);
}

void TemporalScenarioPresenter::on_constraintViewModelCreated(const TemporalConstraintViewModel& constraint_view_model)
{
    auto cst_pres = new TemporalConstraintPresenter{
                                constraint_view_model,
                                m_view,
                                this};
    m_constraints.insert(cst_pres);
    cst_pres->on_zoomRatioChanged(m_zoomRatio);

    m_viewInterface->on_constraintMoved(*cst_pres);

    connect(cst_pres, &TemporalConstraintPresenter::heightPercentageChanged,
            this, [=] () { m_viewInterface->on_constraintMoved(*cst_pres); });
    con(constraint_view_model.model(), &ConstraintModel::startDateChanged,
            this, [=] (const TimeValue&) { m_viewInterface->on_constraintMoved(*cst_pres); });
    connect(cst_pres, &TemporalConstraintPresenter::askUpdate,
            this,     &TemporalScenarioPresenter::on_askUpdate);

    connect(cst_pres, &TemporalConstraintPresenter::constraintHoverEnter,
            [=] () { m_viewInterface->on_hoverOnConstraint(cst_pres->model().id(), true); });
    connect(cst_pres, &TemporalConstraintPresenter::constraintHoverLeave,
            [=] () { m_viewInterface->on_hoverOnConstraint(cst_pres->model().id(), false); });

    // For the state machine
    connect(cst_pres, &TemporalConstraintPresenter::pressed, m_view, &TemporalScenarioView::scenarioPressed);
    connect(cst_pres, &TemporalConstraintPresenter::moved, m_view, &TemporalScenarioView::scenarioMoved);
    connect(cst_pres, &TemporalConstraintPresenter::released, m_view, &TemporalScenarioView::scenarioReleased);
}

void TemporalScenarioPresenter::updateAllElements()
{
    for(auto& constraint : m_constraints)
    {
        m_viewInterface->on_constraintMoved(constraint);
    }

    for(auto& event : m_events)
    {
        m_viewInterface->on_eventMoved(event);
    }

    for(auto& timenode : m_timeNodes)
    {
        m_viewInterface->on_timeNodeMoved(timenode);
    }

    for(auto& state : m_displayedStates)
    {
        m_viewInterface->on_stateMoved(state);
    }
}

#include <Commands/Scenario/Creations/CreateConstraint_State_Event_TimeNode.hpp>
void TemporalScenarioPresenter::handleDrop(const QPointF &pos, const QMimeData *mime)
{
    // If the mime data has states in it we can handle it.
    if(mime->formats().contains(iscore::mime::messagelist()))
    {
        Mime<iscore::MessageList>::Deserializer des{*mime};
        iscore::MessageList ml = des.deserialize();

        MacroCommandDispatcher m(
                    new  Scenario::Command::CreateStateMacro,
                    iscore::IDocument::commandStack(m_layer.processModel()));

        const ScenarioModel& scenar = ::model(m_layer);
        Id<StateModel> createdState;
        auto t = TimeValue::fromMsecs(pos.x() * zoomRatio());
        auto y = pos.y() / (m_view->boundingRect().size().height() + 150);


        auto state = furthestSelectedState(scenar);
        if(state && (scenar.events.at(state->eventId()).date() < t))
        {
            if(state->nextConstraint())
            {
                // We create from the event instead
                auto cmd1 = new Scenario::Command::CreateState{scenar, state->eventId(), y};
                m.submitCommand(cmd1);

                auto cmd2 = new Scenario::Command::CreateConstraint_State_Event_TimeNode{
                            scenar, cmd1->createdState(), t, y};
                m.submitCommand(cmd2);
                createdState = cmd2->createdState();
            }
            else
            {
                auto cmd = new Scenario::Command::CreateConstraint_State_Event_TimeNode{
                           scenar, state->id(), t, state->heightPercentage()};
                m.submitCommand(cmd);
                createdState = cmd->createdState();
            }
        }
        else
        {
            // We create in the emptiness
            auto cmd = new Scenario::Command::CreateTimeNode_Event_State(
                           scenar, t, y);
            m.submitCommand(cmd);
            createdState = cmd->createdState();
        }


        Path<ScenarioModel> scenario_path{scenar};
        auto vecpath = scenario_path.unsafePath().vec();
        vecpath.append({"StateModel", createdState});
        vecpath.append({"MessageItemModel", {}});
        Path<MessageItemModel> state_path{ObjectPath(std::move(vecpath)), {}};

        auto cmd2 = new AddMessagesToState{
                   std::move(state_path),
                   ml};

        m.submitCommand(cmd2);


        m.commit();
    }
}
