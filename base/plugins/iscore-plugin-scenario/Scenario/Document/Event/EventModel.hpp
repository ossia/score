#pragma once
#include <Process/ModelMetadata.hpp>
#include <Process/TimeValue.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <Scenario/Document/VerticalExtent.hpp>
#include <State/Expression.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>
#include <iscore/selection/Selectable.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <qstring.h>
#include <qvector.h>

class DataStream;
class JSONObject;
class QObject;
class StateModel;

namespace OSSIA
{
}
class ScenarioInterface;
class TimeNodeModel;

class EventModel final : public IdentifiedObject<EventModel>
{
        Q_OBJECT
        ISCORE_METADATA("EventModel")

        ISCORE_SERIALIZE_FRIENDS(EventModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(EventModel, JSONObject)

    public:
        /** Public properties of the class **/
        Selectable selection;
        ModelMetadata metadata;
        iscore::ElementPluginModelList pluginModelList;

        static QString description();

        ScenarioInterface* parentScenario() const;

        /** The class **/
        EventModel(const Id<EventModel>&,
                   const Id<TimeNodeModel>& timenode,
                   const VerticalExtent& extent,
                   const TimeValue& date,
                   QObject* parent);

        // Copy
        EventModel(const EventModel& source,
                   const Id<EventModel>&,
                   QObject* parent);

        template<typename DeserializerVisitor>
        EventModel(DeserializerVisitor&& vis, QObject* parent) :
            IdentifiedObject{vis, parent}
        {
            vis.writeTo(*this);
        }

        // Timenode
        void changeTimeNode(const Id<TimeNodeModel>& elt)
        { m_timeNode = elt; }
        const auto& timeNode() const
        { return m_timeNode; }

        // States
        void addState(const Id<StateModel>& ds);
        void removeState(const Id<StateModel>& ds);
        const QVector<Id<StateModel>>& states() const;


        // Other properties
        const iscore::Condition& condition() const;

        VerticalExtent extent() const;

        const TimeValue& date() const;
        void translate(const TimeValue& deltaTime);

        ExecutionStatus status() const;
        void reset();

    public slots:
        void setCondition(const iscore::Condition& arg);

        void setExtent(const VerticalExtent &extent);
        void setDate(const TimeValue& date);

        void setStatus(ExecutionStatus status);

    signals:
        void extentChanged(const VerticalExtent&);
        void dateChanged(const TimeValue&);

        void conditionChanged(const iscore::Condition&);

        void statesChanged();

        void statusChanged(ExecutionStatus status);

    private:
        Id<TimeNodeModel> m_timeNode;

        QVector<Id<StateModel>> m_states;

        iscore::Condition m_condition;

        VerticalExtent m_extent;
        TimeValue m_date{TimeValue::zero()};

        ExecutionStatus m_status{ExecutionStatus::Editing};
};
