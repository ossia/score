#pragma once
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Event/EventModel.hpp>
#include <Scenario/Document/State/StateModel.hpp>
#include <Scenario/Document/TimeNode/TimeNodeModel.hpp>
#include <Scenario/Document/CommentBlock/CommentBlockModel.hpp>
#include <Scenario/Process/ScenarioInterface.hpp>
#include <boost/iterator/indirect_iterator.hpp>
#include <boost/iterator/iterator_facade.hpp>
#include <boost/multi_index/detail/hash_index_iterator.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <iscore/tools/IdentifiedObjectMap.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <QByteArray>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVector>


#include <Scenario/Process/AbstractScenarioLayerModel.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

class DataStream;
class JSONObject;
namespace Process { class LayerModel; }
class ProcessStateDataInterface;
class QEvent;

/**
 * @brief The ScenarioModel class
 *
 * Creation methods return tuples with the identifiers of the objects in their temporal order.
 * (first to last)
 */
namespace Scenario
{
class ScenarioFactory;
class ISCORE_PLUGIN_SCENARIO_EXPORT ScenarioModel final :
        public Process::ProcessModel,
        public ScenarioInterface
{
        Q_OBJECT

        ISCORE_SERIALIZE_FRIENDS(Scenario::ScenarioModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(Scenario::ScenarioModel, JSONObject)
        friend class ScenarioFactory;

    public:
        using layer_type = AbstractScenarioLayerModel;
        ScenarioModel(const TimeValue& duration,
                      const Id<ProcessModel>& id,
                      QObject* parent);
        Scenario::ScenarioModel* clone(
                const Id<ProcessModel>& newId,
                QObject* newParent) const override;
        ~ScenarioModel();

        QString prettyName() const override;

        //// ScenarioModel specifics ////

        // Accessors
        ElementContainer<ConstraintModel> getConstraints() const override
        {
            auto& map = constraints.map().get();
            return {map.begin(), map.end()};
        }

        ElementContainer<StateModel> getStates() const override
        {
            auto& map = states.map().get();
            return {map.begin(), map.end()};
        }

        ElementContainer<EventModel> getEvents() const override
        {
            auto& map = events.map().get();
            return {map.begin(), map.end()};
        }

        ElementContainer<TimeNodeModel> getTimeNodes() const override
        {
            auto& map = timeNodes.map().get();
            return {map.begin(), map.end()};
        }

        ConstraintModel* findConstraint(const Id<ConstraintModel>& id) const override
        {
            return ptr_find(constraints, id);
        }
        EventModel* findEvent(const Id<EventModel>& id) const override
        {
            return ptr_find(events, id);
        }
        TimeNodeModel* findTimeNode(const Id<TimeNodeModel>& id) const override
        {
            return ptr_find(timeNodes, id);
        }
        StateModel* findState(const Id<StateModel>& id) const override
        {
            return ptr_find(states, id);
        }

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
        CommentBlockModel& comment(const Id<CommentBlockModel>& cmtId) const
        {
            return comments.at(cmtId);
        }

        TimeNodeModel& startTimeNode() const override
        {
            return timeNodes.at(m_startTimeNodeId);
        }

        EventModel& startEvent() const
        {
            return events.at(m_startEventId);
        }
        EventModel& endEvent() const
        {
            return events.at(m_endEventId);
        }


        NotifyingMap<ConstraintModel> constraints;
        NotifyingMap<EventModel> events;
        NotifyingMap<TimeNodeModel> timeNodes;
        NotifyingMap<StateModel> states;
        NotifyingMap<CommentBlockModel> comments;

    signals:
        void stateMoved(const StateModel&);
        void eventMoved(const EventModel&);
        void constraintMoved(const ConstraintModel&);
        void commentMoved(const CommentBlockModel&);

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


    private:
        //// ProcessModel specifics ////
        QByteArray makeLayerConstructionData() const override;
        Process::LayerModel* makeLayer_impl(
                const Id<Process::LayerModel>& viewModelId,
                const QByteArray& constructionData,
                QObject* parent) override;

        Process::LayerModel* cloneLayer_impl(
                const Id<Process::LayerModel>& newId,
                const Process::LayerModel& source,
                QObject* parent) override;

        UuidKey<Process::ProcessFactory>concreteFactoryKey() const override;

        void setDurationAndScale(const TimeValue& newDuration) override;
        void setDurationAndGrow(const TimeValue& newDuration) override;
        void setDurationAndShrink(const TimeValue& newDuration) override;

        void startExecution() override;
        void stopExecution() override;
        void reset() override;

        Selection selectableChildren() const override;
public: Selection selectedChildren() const override;
private:void setSelection(const Selection& s) const override;

        ProcessStateDataInterface* startStateData() const override;
        ProcessStateDataInterface* endStateData() const override;

        template<typename Impl>
        ScenarioModel(Deserializer<Impl>& vis, QObject* parent) :
            ProcessModel {vis, parent}
        {
            vis.writeTo(*this);
        }

        Process::LayerModel* loadLayer_impl(
                const VisitorVariant& vis,
                QObject* parent) override;

        void serialize_impl(const VisitorVariant&) const override;

        // To prevent warnings in Clang
        bool event(QEvent* e) override
        {
            return QObject::event(e);
        }

    private:
        template<typename Fun>
        void apply(Fun fun) const {
            fun(&ScenarioModel::constraints);
            fun(&ScenarioModel::states);
            fun(&ScenarioModel::events);
            fun(&ScenarioModel::timeNodes);
            fun(&ScenarioModel::comments);
        }
        ScenarioModel(const Scenario::ScenarioModel& source,
                      const Id<ProcessModel>& id,
                      QObject* parent);
        void makeLayer_impl(AbstractScenarioLayerModel*);

        Id<TimeNodeModel> m_startTimeNodeId {};
        Id<TimeNodeModel> m_endTimeNodeId {};

        Id<EventModel> m_startEventId {};
        Id<EventModel> m_endEventId {};

        Id<StateModel> m_startStateId {};
        // By default, creation in the void will make a constraint
        // that goes to the startEvent and add a new state
};
}
// TODO this ought to go in Selection.hpp ?
template<typename Vector>
QList<const typename Vector::value_type*> selectedElements(const Vector& in)
{
    QList<const typename Vector::value_type*> out;
    for(const auto& elt : in)
    {
        if(elt.selection.get())
            out.append(&elt);
    }

    return out;
}

template<typename T>
QList<const T*> filterSelectionByType(const Selection& sel)
{
    QList<const T*> selected_elements;
    for(auto obj : sel)
    {
        // TODO replace with a virtual Element::type() which will be faster.
        if(auto casted_obj = dynamic_cast<const T*>(obj.data()))
        {
            if(casted_obj->selection.get() && dynamic_cast<Scenario::ScenarioModel*>(casted_obj->parent()))
            {
                selected_elements.push_back(casted_obj);
            }
        }
    }

    return selected_elements;
}


namespace Scenario
{
ISCORE_PLUGIN_SCENARIO_EXPORT const QVector<Id<ConstraintModel>> constraintsBeforeTimeNode(
        const Scenario::ScenarioModel&,
        const Id<TimeNodeModel>& timeNodeId);

const StateModel* furthestSelectedState(const Scenario::ScenarioModel& scenario);
const StateModel* furthestSelectedStateWithoutFollowingConstraint(const Scenario::ScenarioModel& scenario);

inline auto& constraints(const Scenario::ScenarioModel& scenar)
{
    return scenar.constraints;
}
inline auto& events(const Scenario::ScenarioModel& scenar)
{
    return scenar.events;
}
inline auto& timeNodes(const Scenario::ScenarioModel& scenar)
{
    return scenar.timeNodes;
}
inline auto& states(const Scenario::ScenarioModel& scenar)
{
    return scenar.states;
}

template<>
struct ScenarioElementTraits<Scenario::ScenarioModel, ConstraintModel>
{
        static const constexpr auto accessor = static_cast<const NotifyingMap<ConstraintModel>& (*) (const Scenario::ScenarioModel&)>(&constraints);
};
template<>
struct ScenarioElementTraits<Scenario::ScenarioModel, EventModel>
{
        static const constexpr auto accessor = static_cast<const NotifyingMap<EventModel>& (*) (const Scenario::ScenarioModel&)>(&events);
};
template<>
struct ScenarioElementTraits<Scenario::ScenarioModel, TimeNodeModel>
{
        static const constexpr auto accessor = static_cast<const NotifyingMap<TimeNodeModel>& (*) (const Scenario::ScenarioModel&)>(&timeNodes);
};
template<>
struct ScenarioElementTraits<Scenario::ScenarioModel, StateModel>
{
        static const constexpr auto accessor = static_cast<const NotifyingMap<StateModel>& (*) (const Scenario::ScenarioModel&)>(&states);
};

}

DEFAULT_MODEL_METADATA(Scenario::ScenarioModel, "Scenario")
