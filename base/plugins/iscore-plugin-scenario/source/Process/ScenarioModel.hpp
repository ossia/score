#pragma once
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>
#include <Document/State/StateModel.hpp>

#include <ProcessInterface/ProcessModel.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include <ProcessInterface/TimeValue.hpp>
#include <Process/ScenarioInterface.hpp>

#include <iterator>

namespace OSSIA
{
    class Scenario;
}
class TimeNodeModel;
class ConstraintModel;
class EventModel;
class AbstractScenarioLayer;
class ConstraintViewModel;

class OSSIAScenarioImpl;
/**
 * @brief The ScenarioModel class
 *
 * Creation methods return tuples with the identifiers of the objects in their temporal order.
 * (first to last)
 */
class ScenarioModel : public Process, public ScenarioInterface
{
        Q_OBJECT

        ISCORE_SERIALIZE_FRIENDS(ScenarioModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(ScenarioModel, JSONObject)
        friend class ScenarioFactory;

    public:
        using layer_type = AbstractScenarioLayer;
        ScenarioModel(const TimeValue& duration,
                      const id_type<Process>& id,
                      QObject* parent);
        ScenarioModel* clone(
                const id_type<Process>& newId,
                QObject* newParent) const override;

        //// ProcessModel specifics ////
        QByteArray makeViewModelConstructionData() const override;
        LayerModel* makeLayer_impl(
                const id_type<LayerModel>& viewModelId,
                const QByteArray& constructionData,
                QObject* parent) override;

        LayerModel* cloneLayer_impl(
                const id_type<LayerModel>& newId,
                const LayerModel& source,
                QObject* parent) override;

        void setDurationAndScale(const TimeValue& newDuration) override;
        void setDurationAndGrow(const TimeValue& newDuration) override;
        void setDurationAndShrink(const TimeValue& newDuration) override;

        void reset() override;

        Selection selectableChildren() const override;
        Selection selectedChildren() const override;
        void setSelection(const Selection& s) const override;

        QString processName() const override
        { return "Scenario"; }

        ProcessStateDataInterface* startState() const override;
        ProcessStateDataInterface* endState() const override;

        //// ScenarioModel specifics ////
        // Low-level operations (the caller has the responsibility to maintain the consistency of the scenario)
        // The scenario takes ownership.
        void addConstraint(ConstraintModel* constraint);
        void addEvent(EventModel* event);
        void addTimeNode(TimeNodeModel* timeNode);
        void addState(StateModel* state);

        void removeConstraint(ConstraintModel* constraint);
        void removeEvent(EventModel* event);
        void removeTimeNode(TimeNodeModel* timeNode);
        void removeState(StateModel* state);

        // Accessors
        ConstraintModel& constraint(const id_type<ConstraintModel>& constraintId) const override
        {
            return m_constraints.at(constraintId);
        }
        EventModel& event(const id_type<EventModel>& eventId) const override
        {
            return m_events.at(eventId);
        }
        TimeNodeModel& timeNode(const id_type<TimeNodeModel>& timeNodeId) const override
        {
            return m_timeNodes.at(timeNodeId);
        }
        StateModel& state(const id_type<StateModel>& stId) const override
        {
            return m_states.at(stId);
        }

        TimeNodeModel& startTimeNode() const
        {
            return timeNode(m_startTimeNodeId);
        }
        TimeNodeModel& endTimeNode() const
        {
            return timeNode(m_endTimeNodeId);
        }
        EventModel& startEvent() const
        {
            return event(m_startEventId);
        }
        EventModel& endEvent() const
        {
            return event(m_endEventId);
        }

        // Here, a copy is returned because it might be possible
        // to call a method on the scenario (e.g. removeConstraint) that changes the vector
        // while iterating, which would invalidate the iterators
        // and lead to undefined behaviour
        const auto& constraints() const
        {
            return m_constraints;
        }

        const auto& events() const
        {
            return m_events;
        }

        const auto& timeNodes() const
        {
            return m_timeNodes;
        }

        const auto& states() const
        {
            return m_states;
        }

    signals:
        void stateCreated(const StateModel& stateId);
        void eventCreated(const EventModel& eventId);
        void constraintCreated(const ConstraintModel& constraintId);
        void timeNodeCreated(const TimeNodeModel& timeNodeId);

        void stateRemoved(const id_type<StateModel>& stateId);
        void eventRemoved_before(const id_type<EventModel>& eventId);
        void eventRemoved_after(const id_type<EventModel>& eventId);
        void constraintRemoved(const id_type<ConstraintModel>& constraintId);
        void timeNodeRemoved(const id_type<TimeNodeModel>& timeNodeId);

        void stateMoved(const StateModel& stateId);
        void eventMoved(const EventModel& eventId);
        void constraintMoved(const ConstraintModel& constraintId);

        void locked();
        void unlocked();

    public slots:
        void lock()
        {
            emit locked();
        }
        void unlock()
        {
            emit unlocked();
        }

    protected:
        template<typename Impl>
        ScenarioModel(Deserializer<Impl>& vis, QObject* parent) :
            Process {vis, parent}
        {
            vis.writeTo(*this);
        }

        virtual LayerModel* loadLayer_impl(
                const VisitorVariant& vis,
                QObject* parent) override;

        virtual void serialize(const VisitorVariant&) const override;

        // To prevent warnings in Clang
        virtual bool event(QEvent* e) override
        {
            return QObject::event(e);
        }

    private:
        ScenarioModel(const ScenarioModel& source,
                      const id_type<Process>& id,
                      QObject* parent);
        void makeLayer_impl(AbstractScenarioLayer*);

        IdContainer<ConstraintModel> m_constraints;
        IdContainer<EventModel> m_events;
        IdContainer<TimeNodeModel> m_timeNodes;
        IdContainer<StateModel> m_states;

        id_type<TimeNodeModel> m_startTimeNodeId {};
        id_type<TimeNodeModel> m_endTimeNodeId {};

        id_type<EventModel> m_startEventId {};
        id_type<EventModel> m_endEventId {};

        // By default, creation in the void will make a constraint
        // that goes to the startEvent and add a new state

};

#include <iterator>
template<typename Vector>
auto selectedElements(const Vector& in)
    -> QList<const typename Vector::value_type*>
{
    QList<const typename Vector::value_type*> out;
    for(const auto& elt : in)
    {
        if(elt.selection.get())
            out.append(&elt);
    }

    return out;
}

