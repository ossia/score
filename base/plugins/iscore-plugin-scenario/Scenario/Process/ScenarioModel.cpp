#include "ScenarioModel.hpp"

#include "Algorithms/StandardCreationPolicy.hpp"
#include "Algorithms/StandardDisplacementPolicy.hpp"
#include <Scenario/Process/Temporal/TemporalScenarioLayerModel.hpp>

#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>

#include <boost/range/algorithm.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>


ScenarioModel::ScenarioModel(const TimeValue& duration,
                             const Id<Process>& id,
                             QObject* parent) :
    Process {duration, id, "Scenario", parent},
    m_startTimeNodeId{0},
    m_endTimeNodeId{1},
    m_startEventId{0},
    m_endEventId{1}
{
    auto& start_tn = ScenarioCreate<TimeNodeModel>::redo(m_startTimeNodeId, {0.2, 0.8}, TimeValue::zero(), *this);
    auto& end_tn = ScenarioCreate<TimeNodeModel>::redo(m_endTimeNodeId, {0.2, 0.8}, duration, *this);

    ScenarioCreate<EventModel>::redo(m_startEventId, start_tn, {0.4, 0.6}, *this);
    ScenarioCreate<EventModel>::redo(m_endEventId, end_tn, {0.4, 0.6}, *this);

    // At the end because plug-ins depend on the start/end timenode & al being here
    pluginModelList = new iscore::ElementPluginModelList{iscore::IDocument::documentFromObject(parent), this};
    metadata.setName(QString("Scenario.%1").arg(*this->id().val()));
}

ScenarioModel::ScenarioModel(const ScenarioModel& source,
                             const Id<Process>& id,
                             QObject* parent) :
    Process {source, id, "Scenario", parent},
    m_startTimeNodeId{source.m_startTimeNodeId},
    m_endTimeNodeId{source.m_endTimeNodeId},
    m_startEventId{source.m_startEventId},
    m_endEventId{source.m_endEventId}
{
    pluginModelList = new iscore::ElementPluginModelList(*source.pluginModelList, this);

    // This almost terrifying piece of code will simply clone
    // all the elements (constraint, etc...) from the source to this class
    // without duplicating code too much.
    apply([&] (const auto& m) {
        using the_class = typename remove_qualifs_t<decltype(this->*m)>::value_type;
        for(const auto& elt : source.*m)
            (this->*m).add(new the_class{elt, elt.id(), this});
    });
    metadata.setName(QString("Scenario.%1").arg(*this->id().val()));
}

ScenarioModel* ScenarioModel::clone(
        const Id<Process>& newId,
        QObject* newParent) const
{
    return new ScenarioModel {*this, newId, newParent};
}

QByteArray ScenarioModel::makeLayerConstructionData() const
{
    // For all existing constraints we need to generate corresponding
    // view models ids. One day we may need to do this for events / time nodes too.
    QMap<Id<ConstraintModel>, Id<ConstraintViewModel>> map;
    std::vector<Id<ConstraintViewModel>> vec;
    vec.reserve(constraints.size());
    for(const auto& constraint : constraints)
    {
        auto id = getStrongId(vec);
        vec.push_back(id);
        map.insert(constraint.id(), id);
    }

    QByteArray arr;
    QDataStream s{&arr, QIODevice::WriteOnly};
    s << map;
    return arr;
}

LayerModel* ScenarioModel::makeLayer_impl(
        const Id<LayerModel>& viewModelId,
        const QByteArray& constructionData,
        QObject* parent)
{
    QMap<Id<ConstraintModel>, Id<ConstraintViewModel>> map;
    QDataStream s{constructionData};
    s >> map;

    auto scen = new TemporalScenarioLayerModel {viewModelId, map, *this, parent};
    makeLayer_impl(scen);
    return scen;
}


LayerModel* ScenarioModel::cloneLayer_impl(
        const Id<LayerModel>& newId,
        const LayerModel& source,
        QObject* parent)
{
    auto scen = new TemporalScenarioLayerModel{
                static_cast<const TemporalScenarioLayerModel&>(source),
                newId,
                *this,
                parent};
    makeLayer_impl(scen);
    return scen;
}

const ProcessFactoryKey& ScenarioModel::key() const
{
    static const ProcessFactoryKey name{"Scenario"};
    return name;
}

QString ScenarioModel::prettyName() const
{
    return metadata.name();
}

void ScenarioModel::setDurationAndScale(const TimeValue& newDuration)
{
    double scale =  newDuration / duration();

    for(auto& timenode : timeNodes)
    {
        timenode.setDate(timenode.date() * scale);
        // Since events will also move we do not need
        // to move the timenode.
    }

    for(auto& event : events)
    {
        event.setDate(event.date() * scale);
        emit eventMoved(event);
    }

    for(auto& constraint : constraints)
    {
        constraint.setStartDate(constraint.startDate() * scale);
        // Note : scale the min / max.

        ConstraintDurations::Algorithms::changeAllDurations(
                    constraint,
                    constraint.duration.defaultDuration() * scale);

        for(auto& process : constraint.processes)
        {
            process.setDurationAndScale(constraint.duration.defaultDuration() * scale);
        }

        emit constraintMoved(constraint);
    }

    this->setDuration(newDuration);
}

void ScenarioModel::setDurationAndGrow(const TimeValue& newDuration)
{
    ///* Should work but does not ?
    /*StandardDisplacementPolicy::setEventPosition(*this,
                                                 endEvent()->id(),
                                                 newDuration,
                                                 endEvent()->heightPercentage(),
                                                 [&] (ProcessModel* p, const TimeValue& t)
     { p->expandProcess(ExpandMode::Grow, t); }); */

    auto& eev = endEvent();

    eev.setDate(newDuration);
    timeNode(eev.timeNode()).setDate(newDuration);
    emit eventMoved(eev);
    this->setDuration(newDuration);
}

void ScenarioModel::setDurationAndShrink(const TimeValue& newDuration)
{
    return; // Disabled by Asana

    ///* Should work but does not ?
    /* StandardDisplacementPolicy::setEventPosition(*this,
                                                 endEvent()->id(),
                                                 newDuration,
                                                 endEvent()->heightPercentage(),
                                                 [&] (ProcessModel* p, const TimeValue& t)
     { p->expandProcess(ExpandMode::Grow, t); }); */

    /*
    auto& eev = endEvent();

    eev.setDate(newDuration);
    timeNode(eev.timeNode()).setDate(newDuration);
    emit eventMoved(eev.id());
    this->setDuration(newDuration);
    */
}

void ScenarioModel::startExecution()
{
    emit execution(true);
    for(auto& constraint : constraints)
    {
        constraint.startExecution();
    }
}

void ScenarioModel::stopExecution()
{
    emit execution(false);
}

void ScenarioModel::reset()
{
    for(auto& constraint : constraints)
    {
        constraint.reset();
    }

    for(auto& event : events)
    {
        event.reset();
    }

    // TODO reset events / states display too
}

Selection ScenarioModel::selectableChildren() const
{
    Selection objects;
    apply([&] (const auto& m) {
        for(auto& elt : this->*m)
            objects.append(&elt);
    });
    return objects;
}

template<typename InputVec, typename OutputVec>
static void copySelected(const InputVec& in, OutputVec& out)
{
    for(const auto& elt : in)
    {
        if(elt.selection.get())
            out.append(&elt);
    }
}

Selection ScenarioModel::selectedChildren() const
{
    Selection objects;
    apply([&] (const auto& m) { copySelected(this->*m, objects); });
    return objects;
}

void ScenarioModel::setSelection(const Selection& s) const
{
    // OPTIMIZEME
    apply([&] (auto&& m) {
        for(auto& elt : this->*m)
            elt.selection.set(s.contains(&elt));
    });
}

ProcessStateDataInterface* ScenarioModel::startState() const
{
    ISCORE_TODO;
    return nullptr;
}

ProcessStateDataInterface* ScenarioModel::endState() const
{
    ISCORE_TODO;
    return nullptr;
}

const QVector<Id<ConstraintModel> > ScenarioModel::constraintsBeforeTimeNode(const Id<TimeNodeModel>& timeNodeId) const
{
    QVector<Id<ConstraintModel>> cstrs;
    auto& tn = timeNode(timeNodeId);
    for(auto& ev : tn.events())
    {
        auto& evM = event(ev);
        for (auto& st : evM.states())
        {
            auto& stM = state(st);
            if(stM.previousConstraint())
                cstrs.push_back(stM.previousConstraint());
        }
    }
    return cstrs;
}


void ScenarioModel::makeLayer_impl(AbstractScenarioLayerModel* scen)
{
    // There is no ConstraintCreated connection to the layer,
    // because the constraints view models are created
    // from the commands, since they require ids too.
    con(constraints, &NotifyingMap<ConstraintModel>::removed,
        scen, &AbstractScenarioLayerModel::on_constraintRemoved);

    con(states, &NotifyingMap<StateModel>::added,
        scen, &AbstractScenarioLayerModel::stateCreated);
    con(states, &NotifyingMap<StateModel>::removed,
        scen, &AbstractScenarioLayerModel::stateRemoved);

    con(events, &NotifyingMap<EventModel>::added,
        scen, &AbstractScenarioLayerModel::eventCreated);
    con(events, &NotifyingMap<EventModel>::removed,
        scen, &AbstractScenarioLayerModel::eventRemoved);

    con(timeNodes, &NotifyingMap<TimeNodeModel>::added,
        scen, &AbstractScenarioLayerModel::timeNodeCreated);
    con(timeNodes, &NotifyingMap<TimeNodeModel>::removed,
        scen, &AbstractScenarioLayerModel::timeNodeRemoved);

    connect(this, &ScenarioModel::eventMoved,
            scen, &AbstractScenarioLayerModel::eventMoved);

    connect(this, &ScenarioModel::constraintMoved,
            scen, &AbstractScenarioLayerModel::constraintMoved);
}

const StateModel* furthestSelectedState(const ScenarioModel& scenar)
{
    const StateModel* furthest_state{};
    {
        TimeValue max_t = TimeValue::zero();
        double max_y = 0;
        for(StateModel& state : scenar.states)
        {
            if(state.selection.get())
            {
                auto date = scenar.events.at(state.eventId()).date();
                if(!furthest_state || date > max_t)
                {
                    max_t = date;
                    max_y = state.heightPercentage();
                    furthest_state = &state;
                }
                else if(date == max_t && state.heightPercentage() > max_y)
                {
                    max_y = state.heightPercentage();
                    furthest_state = &state;
                }
            }
        }
        if(furthest_state)
        {
            return furthest_state;
        }
    }

    // If there is no furthest state, we instead go for a constraint
    const ConstraintModel* furthest_constraint{};
    {
        TimeValue max_t = TimeValue::zero();
        double max_y = 0;
        for(ConstraintModel& cst : scenar.constraints)
        {
            if(cst.selection.get())
            {
                auto date = cst.duration.defaultDuration();
                if(!furthest_constraint || date > max_t)
                {
                    max_t = date;
                    max_y = cst.heightPercentage();
                    furthest_constraint = &cst;
                }
                else if (date == max_t && cst.heightPercentage() > max_y)
                {
                    max_y = cst.heightPercentage();
                    furthest_constraint = &cst;
                }
            }
        }

        if (furthest_constraint)
        {
            return &scenar.states.at(furthest_constraint->endState());
        }
    }

    return nullptr;
}

const StateModel*furthestSelectedStateWithoutFollowingConstraint(const ScenarioModel& scenar)
{
    const StateModel* furthest_state{};
    {
        TimeValue max_t = TimeValue::zero();
        double max_y = 0;
        for(StateModel& state : scenar.states)
        {
            if(state.selection.get() && !state.nextConstraint())
            {
                auto date = scenar.events.at(state.eventId()).date();
                if(!furthest_state || date > max_t)
                {
                    max_t = date;
                    max_y = state.heightPercentage();
                    furthest_state = &state;
                }
                else if(date == max_t && state.heightPercentage() > max_y)
                {
                    max_y = state.heightPercentage();
                    furthest_state = &state;
                }
            }
        }
        if(furthest_state)
        {
            return furthest_state;
        }
    }

    // If there is no furthest state, we instead go for a constraint
    const ConstraintModel* furthest_constraint{};
    {
        TimeValue max_t = TimeValue::zero();
        double max_y = 0;
        for(ConstraintModel& cst : scenar.constraints)
        {
            if(cst.selection.get())
            {
                const auto& state = scenar.states.at(cst.endState());
                if(state.nextConstraint())
                    continue;

                auto date = cst.duration.defaultDuration();
                if(!furthest_constraint || date > max_t)
                {
                    max_t = date;
                    max_y = cst.heightPercentage();
                    furthest_constraint = &cst;
                }
                else if (date == max_t && cst.heightPercentage() > max_y)
                {
                    max_y = cst.heightPercentage();
                    furthest_constraint = &cst;
                }
            }
        }

        if (furthest_constraint)
        {
            return &scenar.states.at(furthest_constraint->endState());
        }
    }

    return nullptr;
}
