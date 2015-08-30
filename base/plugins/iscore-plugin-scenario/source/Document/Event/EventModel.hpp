#pragma once
#include <source/Document/ModelMetadata.hpp>
#include <source/Document/VerticalExtent.hpp>

#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/selection/Selectable.hpp>
#include <ProcessInterface/TimeValue.hpp>

#include <State/State.hpp>
#include <State/Expression.hpp>

#include "source/Document/State/StateModel.hpp"
#include "source/Document/Event/EventStatus.hpp"
#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>
namespace OSSIA
{
    class TimeNode;
}
class ConstraintModel;
class TimeNodeModel;
class ScenarioInterface;

class EventModel : public IdentifiedObject<EventModel>
{
        Q_OBJECT
        ISCORE_METADATA("EventModel")

        Q_PROPERTY(iscore::Condition condition
                   READ condition
                   WRITE setCondition
                   NOTIFY conditionChanged)

        Q_PROPERTY(iscore::Trigger trigger
                   READ trigger
                   WRITE setTrigger
                   NOTIFY triggerChanged)

        Q_PROPERTY(EventStatus status
                   READ status
                   WRITE setStatus
                   NOTIFY statusChanged)

        ISCORE_SERIALIZE_FRIENDS(EventModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(EventModel, JSONObject)

    public:
        /** Public properties of the class **/
        Selectable selection;
        ModelMetadata metadata;
        iscore::ElementPluginModelList pluginModelList;

        static QString prettyName();

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
        void changeTimeNode(const Id<TimeNodeModel>&);
        const Id<TimeNodeModel>& timeNode() const;

        // States
        void addState(const Id<StateModel>& ds);
        void removeState(const Id<StateModel>& ds);
        const QVector<Id<StateModel>>& states() const;


        // Other properties
        const iscore::Condition& condition() const;
        const iscore::Trigger& trigger() const;

        VerticalExtent extent() const;

        const TimeValue& date() const;
        void translate(const TimeValue& deltaTime);


        EventStatus status() const;
        void reset();

    public slots:
        void setCondition(const iscore::Condition& arg);
        void setTrigger(const iscore::Trigger& trigger);

        void setExtent(const VerticalExtent &extent);
        void setDate(const TimeValue& date);

        void setStatus(EventStatus status);

    signals:
        void extentChanged(const VerticalExtent&);
        void dateChanged(const TimeValue&);

        void conditionChanged(const iscore::Condition&);
        void triggerChanged(const iscore::Trigger&);

        void statesChanged();

        void statusChanged(EventStatus status);

    private:
        Id<TimeNodeModel> m_timeNode {};

        QVector<Id<StateModel>> m_states;

        iscore::Condition m_condition;
        iscore::Trigger m_trigger; // TODO in timenode?

        VerticalExtent m_extent;
        TimeValue m_date{TimeValue::zero()};

        EventStatus m_status{EventStatus::Editing};
};
