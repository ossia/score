#pragma once
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>
#include <Document/State/StateModel.hpp>

#include <ProcessInterface/Process.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include <ProcessInterface/TimeValue.hpp>
#include <Process/ScenarioInterface.hpp>

#include <iscore/tools/NotifyingMap.hpp>

#include <iterator>

namespace OSSIA
{
    class Scenario;
}
class TimeNodeModel;
class ConstraintModel;
class EventModel;
class AbstractScenarioLayerModel;
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
        using layer_type = AbstractScenarioLayerModel;
        ScenarioModel(const TimeValue& duration,
                      const Id<Process>& id,
                      QObject* parent);
        ScenarioModel* clone(
                const Id<Process>& newId,
                QObject* newParent) const override;

        //// ProcessModel specifics ////
        QByteArray makeViewModelConstructionData() const override;
        LayerModel* makeLayer_impl(
                const Id<LayerModel>& viewModelId,
                const QByteArray& constructionData,
                QObject* parent) override;

        LayerModel* cloneLayer_impl(
                const Id<LayerModel>& newId,
                const LayerModel& source,
                QObject* parent) override;

        void setDurationAndScale(const TimeValue& newDuration) override;
        void setDurationAndGrow(const TimeValue& newDuration) override;
        void setDurationAndShrink(const TimeValue& newDuration) override;

        void startExecution() override;
        void stopExecution() override;
        void reset() override;

        Selection selectableChildren() const override;
        Selection selectedChildren() const override;
        void setSelection(const Selection& s) const override;

        QString processName() const override
        { return "Scenario"; }

        ProcessStateDataInterface* startState() const override;
        ProcessStateDataInterface* endState() const override;

        //// ScenarioModel specifics ////

        // Accessors
        ConstraintModel& constraint(const Id<ConstraintModel>& constraintId) const override
        {
            return constraints.at(constraintId);
        }
        EventModel& event(const Id<EventModel>& eventId) const override
        {
            return events.at(eventId);
        }
        TimeNodeModel& timeNode(const Id<TimeNodeModel>& timeNodeId) const override
        {
            return timeNodes.at(timeNodeId);
        }
        StateModel& state(const Id<StateModel>& stId) const override
        {
            return states.at(stId);
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

        NotifyingMap<ConstraintModel> constraints;
        NotifyingMap<EventModel> events;
        NotifyingMap<TimeNodeModel> timeNodes;
        NotifyingMap<StateModel> states;

    signals:
        void stateMoved(const StateModel&);
        void eventMoved(const EventModel&);
        void constraintMoved(const ConstraintModel&);

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
        template<typename Fun>
        void apply(Fun fun) const {
            fun(&ScenarioModel::timeNodes);
            fun(&ScenarioModel::events);
            fun(&ScenarioModel::constraints);
            fun(&ScenarioModel::states);
        }
        ScenarioModel(const ScenarioModel& source,
                      const Id<Process>& id,
                      QObject* parent);
        void makeLayer_impl(AbstractScenarioLayerModel*);

        Id<TimeNodeModel> m_startTimeNodeId {};
        Id<TimeNodeModel> m_endTimeNodeId {};

        Id<EventModel> m_startEventId {};
        Id<EventModel> m_endEventId {};

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

