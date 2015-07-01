#include "EventPresenter.hpp"

#include "Document/Event/EventModel.hpp"
#include "Document/Event/EventView.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>

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
            this,   &EventPresenter::triggerSetted) ;

    connect(&m_model, &EventModel::localStatesChanged,
            this,    &EventPresenter::updateStateView);

    connect(m_view, &EventView::eventHoverEnter,
            this,   &EventPresenter::eventHoverEnter);

    connect(m_view, &EventView::eventHoverLeave,
            this,   &EventPresenter::eventHoverLeave);

    connect(m_view, &EventView::dropReceived,
            this, &EventPresenter::handleDrop);

    m_view->setCondition(m_model.condition());
    m_view->setTrigger(m_model.trigger());
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
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintView.hpp"
#include "Document/State/StateView.hpp"
#include "Process/Temporal/TemporalScenarioPresenter.hpp"
#include "ProcessInterface/ProcessModel.hpp"

// TODO Is it the good place to do that ?

void EventPresenter::updateStateView() const
{
    // TODO First check if the event has states.

    auto scenar = m_model.parentScenario();
    const auto& constraints_models = scenar->constraints();

    auto scenar_pres = static_cast<TemporalScenarioPresenter*>(this->parent());
    const auto& constraints_presenters = scenar_pres->constraints();

    // Then check the constraints.
    for(const auto& constraint : m_model.previousConstraints())
    {
        /// Checking
        auto constraint_it = constraints_models.find(constraint);
        if(constraint_it == constraints_models.end())
        {
            // We're still constructing and all the "required" constraints are
            // not here; we have to stop.
            // This will be called anyway when the last constraint is added.
            return;
        }

        auto constraint_pres_it = constraints_presenters.find(constraint);
        if(constraint_pres_it == constraints_presenters.end())
        {
            return;
        }

        auto cstr_pres = *constraint_pres_it;
        const auto& procs = (*constraint_it)->processes();

        /// Applying
        if(std::any_of(
                    procs.begin(),
                    procs.end(),
                    [] (ProcessModel* proc) { return proc->endState() != nullptr; }))
        {
// TODO state arent here
//            ::view(cstr_pres)->startState()->setContainMessage(true);
        }
        else
        {
//            ::view(cstr_pres)->startState()->setContainMessage(false);
        }
    }

    for(const auto& constraint : m_model.nextConstraints())
    {
        /// Checking
        auto constraint_it = constraints_models.find(constraint);
        if(constraint_it == constraints_models.end())
        {
            // We're still constructing and all the "required" constraints are
            // not here; we have to stop.
            // This will be called anyway when the last constraint is added.
            return;
        }

        auto constraint_pres_it = constraints_presenters.find(constraint);
        if(constraint_pres_it == constraints_presenters.end())
        {
            return;
        }

        auto cstr_pres = *constraint_pres_it;
        const auto& procs = (*constraint_it)->processes();

        /// Applying
        if(std::any_of(
                    procs.begin(),
                    procs.end(),
                    [] (ProcessModel* proc) { return proc->startState() != nullptr; }))
        {
// TODO states arent here
//            ::view(cstr_pres)->startState()->setContainMessage(true);
        }
        else
        {
//            ::view(cstr_pres)->startState()->setContainMessage(false);
        }
    }
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
    const auto& constraints = scenar->constraints();
    for(const auto& constraint : ids)
    {
        auto constraint_it = constraints.find(constraint);
        if(constraint_it == constraints.end())
        {
            // We're constructing something
            // so this function will get called again in few milliseconds
            return;
        }

        const auto& cstr = scenar->constraint(constraint);
        connections.append(
                    connect(&cstr, &ConstraintModel::processesChanged,
                            this,  &EventPresenter::updateStateView));

    }

    updateStateView();
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

void EventPresenter::updateMinExtremities(const id_type<ConstraintModel> &cstr, const double y)
{
    m_extremityMin = {cstr, y};
}

void EventPresenter::updateMaxExtremities(const id_type<ConstraintModel> &cstr, const double y)
{
    m_extremityMax = {cstr, y};
}

const QPair<id_type<ConstraintModel>, double> EventPresenter::extremityMin() const
{
    return m_extremityMin;
}

const QPair<id_type<ConstraintModel>, double> EventPresenter::extremityMax() const
{
    return m_extremityMax;
}

void EventPresenter::triggerSetted(QString trig)
{
    m_view->setTrigger(trig);
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
        iscore::State s;
        deser.writeTo(s);

        auto cmd = new Scenario::Command::AddStateToEvent{
                iscore::IDocument::path(m_model),
                std::move(s)};
        m_dispatcher.submitCommand(cmd);
    }
}

