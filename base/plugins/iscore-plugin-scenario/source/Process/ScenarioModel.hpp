#pragma once
#include <Document/Constraint/ConstraintModel.hpp>
#include <Document/Event/EventModel.hpp>
#include <Document/TimeNode/TimeNodeModel.hpp>
#include <Document/State/DisplayedStateModel.hpp>

#include <ProcessInterface/ProcessModel.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>

#include <ProcessInterface/TimeValue.hpp>

#include <iterator>

namespace OSSIA
{
    class Scenario;
}
class TimeNodeModel;
class ConstraintModel;
class EventModel;
class AbstractScenarioLayer;
class AbstractConstraintViewModel;

class OSSIAScenarioImpl;
/**
 * @brief The ScenarioModel class
 *
 * Creation methods return tuples with the identifiers of the objects in their temporal order.
 * (first to last)
 */
class ScenarioModel : public ProcessModel
{
        Q_OBJECT

        ISCORE_SERIALIZE_FRIENDS(ScenarioModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(ScenarioModel, JSONObject)
        friend class ScenarioFactory;

    public:
        using layer_type = AbstractScenarioLayer;

        ScenarioModel(const TimeValue& duration,
                      const id_type<ProcessModel>& id,
                      QObject* parent);
        ScenarioModel* clone(
                const id_type<ProcessModel>& newId,
                QObject* newParent) override;

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
        void addDisplayedState(DisplayedStateModel* state);

        void removeConstraint(ConstraintModel* constraint);
        void removeEvent(EventModel* event);
        void removeTimeNode(TimeNodeModel* timeNode);
        //TODO state removeState

        // Accessors
        ConstraintModel& constraint(const id_type<ConstraintModel>& constraintId) const;
        EventModel& event(const id_type<EventModel>& eventId) const;
        TimeNodeModel& timeNode(const id_type<TimeNodeModel>& timeNodeId) const;
        DisplayedStateModel& displayedState(const id_type<DisplayedStateModel>& stId) const;

        TimeNodeModel& startTimeNode() const;
        TimeNodeModel& endTimeNode() const;
        EventModel& startEvent() const;
        EventModel& endEvent() const;

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

        const auto& dispStates() const
        {
            return m_displayedStates;
        }

    signals:
        void eventCreated(const id_type<EventModel>& eventId);
        void constraintCreated(const id_type<ConstraintModel>& constraintId);
        void timeNodeCreated(const id_type<TimeNodeModel>& timeNodeId);

        void eventRemoved_before(const id_type<EventModel>& eventId);
        void eventRemoved_after(const id_type<EventModel>& eventId);
        void constraintRemoved(const id_type<ConstraintModel>& constraintId);
        void timeNodeRemoved(const id_type<TimeNodeModel>& timeNodeId);

        void eventMoved(const id_type<EventModel>& eventId);
        void constraintMoved(const id_type<ConstraintModel>& constraintId);

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
            ProcessModel {vis, parent}
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
                      const id_type<ProcessModel>& id,
                      QObject* parent);
        void makeLayer_impl(layer_type*);

        IdContainer<ConstraintModel> m_constraints;
        IdContainer<EventModel> m_events;
        IdContainer<TimeNodeModel> m_timeNodes;
        IdContainer<DisplayedStateModel> m_displayedStates;

        id_type<TimeNodeModel> m_startTimeNodeId {};
        id_type<TimeNodeModel> m_endTimeNodeId {};

        id_type<EventModel> m_startEventId {};
        id_type<EventModel> m_endEventId {};
};

#include <iterator>
template<typename Vector>
Vector selectedElements(const Vector& in)
{
    Vector out;
    for(const auto& elt : in)
    {
        if(elt->selection.get())
            out.insert(elt);
    }

    return out;
}

