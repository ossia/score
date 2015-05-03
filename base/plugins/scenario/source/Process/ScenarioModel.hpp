#pragma once
#include "ProcessInterface/ProcessModel.hpp"
#include <iscore/tools/SettableIdentifier.hpp>

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
class AbstractScenarioViewModel;
class AbstractConstraintViewModel;

/**
 * @brief The ScenarioModel class
 *
 * Creation methods return tuples with the identifiers of the objects in their temporal order.
 * (first to last)
 */
class ScenarioModel : public ProcessModel
{
        Q_OBJECT

        friend void Visitor<Reader<DataStream>>::readFrom<ScenarioModel> (const ScenarioModel&);
        friend void Visitor<Writer<DataStream>>::writeTo<ScenarioModel> (ScenarioModel&);
        friend void Visitor<Reader<JSONObject>>::readFrom<ScenarioModel> (const ScenarioModel&);
        friend void Visitor<Writer<JSONObject>>::writeTo<ScenarioModel> (ScenarioModel&);
        friend class ScenarioFactory;

    public:
        using view_model_type = AbstractScenarioViewModel;

        ScenarioModel(const TimeValue& duration,
                      const id_type<ProcessModel>& id,
                      QObject* parent);
        ProcessModel* clone(
                const id_type<ProcessModel>& newId,
                QObject* newParent) override;

        //// ProcessModel specifics ////
        QByteArray makeViewModelConstructionData() const override;
        ProcessViewModel* makeViewModel_impl(
                const id_type<ProcessViewModel>& viewModelId,
                const QByteArray& constructionData,
                QObject* parent) override;

        ProcessViewModel* cloneViewModel_impl(
                const id_type<ProcessViewModel>& newId,
                const ProcessViewModel& source,
                QObject* parent) override;

        void setDurationAndScale(const TimeValue& newDuration) override;
        void setDurationAndGrow(const TimeValue& newDuration) override;
        void setDurationAndShrink(const TimeValue& newDuration) override;

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

        void removeConstraint(ConstraintModel* constraint);
        void removeEvent(EventModel* event);
        void removeTimeNode(TimeNodeModel* timeNode);

        // Accessors
        ConstraintModel& constraint(const id_type<ConstraintModel>& constraintId) const;
        EventModel& event(const id_type<EventModel>& eventId) const;
        TimeNodeModel& timeNode(const id_type<TimeNodeModel>& timeNodeId) const;

        EventModel& startEvent() const;
        EventModel& endEvent() const;

        // Here, a copy is returned because it might be possible
        // to call a method on the scenario (e.g. removeConstraint) that changes the vector
        // while iterating, which would invalidate the iterators
        // and lead to undefined behaviour
        std::vector<ConstraintModel*> constraints() const
        {
            return m_constraints;
        }

        std::vector<EventModel*> events() const
        {
            return m_events;
        }

        std::vector<TimeNodeModel*> timeNodes() const
        {
            return m_timeNodes;
        }

    signals:
        void eventCreated(const id_type<EventModel>& eventId);
        void constraintCreated(const id_type<ConstraintModel>& constraintId);
        void timeNodeCreated(const id_type<TimeNodeModel>& timeNodeId);

        void eventRemoved(const id_type<EventModel>& eventId);
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

        virtual ProcessViewModel* loadViewModel_impl(
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
        void makeViewModel_impl(view_model_type*);

        std::vector<ConstraintModel*> m_constraints;
        std::vector<EventModel*> m_events;
        std::vector<TimeNodeModel*> m_timeNodes;

        id_type<EventModel> m_startEventId {};
        id_type<EventModel> m_endEventId {};
};


template<typename Vector>
Vector selectedElements(const Vector& in)
{
    Vector out;
    std::copy_if(std::begin(in),
                 std::end(in),
                 back_inserter(out),
                 [](const typename Vector::value_type& c)
    {
        return c->selection.get();
    });

    return out;
}

