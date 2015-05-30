#include "EventPresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventView.hpp"

#include <QGraphicsScene>

EventPresenter::EventPresenter(const EventModel& model,
                               QGraphicsObject* parentview,
                               QObject* parent) :
    NamedObject {"EventPresenter", parent},
    m_model {model},
    m_view {new EventView{*this, parentview}},
    m_dispatcher{iscore::IDocument::documentFromObject(m_model)->commandStack()}
{
    // The scenario catches this :
    connect(&m_model.selection, &Selectable::changed,
            m_view, &EventView::setSelected);

    connect(&(m_model.metadata),  &ModelMetadata::colorChanged,
            m_view,                 &EventView::changeColor);

    connect(&m_model, &EventModel::previousConstraintsChanged,
            this,   &EventPresenter::on_previousConstraintsChanged);
    connect(&m_model, &EventModel::nextConstraintsChanged,
            this,   &EventPresenter::on_nextConstraintsChanged);

    connect(&m_model, &EventModel::heightPercentageChanged,
            this,    &EventPresenter::heightPercentageChanged);
    connect(&m_model, &EventModel::conditionChanged,
            m_view,  &EventView::setCondition);
    connect(&m_model, &EventModel::triggerChanged,
            m_view,  &EventView::setTrigger);

    connect(&m_model, &EventModel::localStatesChanged,
            this,    &EventPresenter::updateViewHalves);

    connect(m_view, &EventView::eventHoverEnter,
            this,   &EventPresenter::eventHoverEnter);

    connect(m_view, &EventView::eventHoverLeave,
            this,   &EventPresenter::eventHoverLeave);

    connect(m_view, &EventView::dropReceived,
            this, &EventPresenter::handleDrop);
}

EventPresenter::~EventPresenter()
{
    if(m_view)
    {
        auto sc = m_view->scene();

        if(sc)
        {
            sc->removeItem(m_view);
        }

        m_view->deleteLater();
    }
}

const id_type<EventModel>& EventPresenter::id() const
{
    return m_model.id();
}

EventView* EventPresenter::view() const
{
    return m_view;
}

const EventModel& EventPresenter::model() const
{
    return m_model;
}

bool EventPresenter::isSelected() const
{
    return m_model.selection.get();
}

#include "Process/ScenarioModel.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "ProcessInterface/ProcessModel.hpp"
void EventPresenter::updateViewHalves() const
{
    // First check if the event has states.
    int halves = 0;
    if(!m_model.states().empty())
        halves |= EventView::Halves::After;

    auto scenar = m_model.parentScenario();
    // Then check the constraints.
    for(const auto& constraint : m_model.previousConstraints())
    {
        const auto& procs = scenar->constraint(constraint).processes();
        if(std::any_of(
                    procs.begin(),
                    procs.end(),
                    [] (ProcessModel* proc) { return proc->endState() != nullptr; }))
        {
            halves |= EventView::Halves::Before;
        }
    }

    for(const auto& constraint : m_model.nextConstraints())
    {
        const auto& procs = scenar->constraint(constraint).processes();
        if(std::any_of(
                    procs.begin(),
                    procs.end(),
                    [] (ProcessModel* proc) { return proc->startState() != nullptr; }))
        {
            halves |= EventView::Halves::After;
        }
    }

    m_view->setHalves((EventView::Halves) halves);
}

void EventPresenter::constraintsChangedHelper(
        const QVector<id_type<ConstraintModel> > &ids,
        QVector<QMetaObject::Connection> &connections)
{
    for(auto& conn : connections)
    {
        this->disconnect(conn);
    }
    connections.clear();

    auto scenar = m_model.parentScenario();
    for(const auto& constraint : ids)
    {
        const auto& cstr = scenar->constraint(constraint);
        connections.append(
                    connect(&cstr, &ConstraintModel::processesChanged,
                            this,  &EventPresenter::updateViewHalves));

    }

    updateViewHalves();
}

void EventPresenter::on_previousConstraintsChanged()
{
    constraintsChangedHelper(m_model.previousConstraints(),
                             m_previousConstraintsConnections);
}

void EventPresenter::on_nextConstraintsChanged()
{
    constraintsChangedHelper(m_model.nextConstraints(),
                             m_nextConstraintsConnections);
}

#include "Commands/Event/AddStateToEvent.hpp"
#include <QMimeData>
#include <QJsonDocument>
#include <iscore/document/DocumentInterface.hpp>
void EventPresenter::handleDrop(const QMimeData *mime)
{
    // If the mime data has states in it we can handle it.
    if(mime->formats().contains("application/x-iscore-state"))
    {
        Deserializer<JSONObject> deser{
            QJsonDocument::fromJson(mime->data("application/x-iscore-state")).object()};
        State s;
        deser.writeTo(s);

        auto cmd = new Scenario::Command::AddStateToEvent{
                iscore::IDocument::path(m_model),
                std::move(s)};
        m_dispatcher.submitCommand(cmd);
    }
}

