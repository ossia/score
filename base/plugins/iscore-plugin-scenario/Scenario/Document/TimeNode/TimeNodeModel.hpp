#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <Scenario/Document/VerticalExtent.hpp>
#include <Process/ModelMetadata.hpp>
#include <Process/TimeValue.hpp>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/selection/Selectable.hpp>

#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>
class EventModel;
class TriggerModel;
class ScenarioInterface;
class TimeNodeModel : public IdentifiedObject<TimeNodeModel>
{
        Q_OBJECT
        ISCORE_METADATA("TimeNodeModel")

        ISCORE_SERIALIZE_FRIENDS(TimeNodeModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(TimeNodeModel, JSONObject)

    public:
        /** Properties of the class **/
        Selectable selection;
        ModelMetadata metadata;
        iscore::ElementPluginModelList pluginModelList;

        static QString prettyName()
        { return QObject::tr("Time Node"); }


        /** The class **/
        TimeNodeModel(
                const Id<TimeNodeModel>& id,
                const VerticalExtent& extent,
                const TimeValue& date,
                QObject* parent);

        template<typename DeserializerVisitor>
        TimeNodeModel(DeserializerVisitor&& vis, QObject* parent) :
            IdentifiedObject{vis, parent}
        {
            vis.writeTo(*this);
        }

        TimeNodeModel(
                const TimeNodeModel& source,
                const Id<TimeNodeModel>& id,
                QObject* parent);


        // Utility
        ScenarioInterface* parentScenario() const;

        // Data of the TimeNode
        const VerticalExtent& extent() const;
        void setExtent(const VerticalExtent &extent);

        const TimeValue& date() const;
        void setDate(const TimeValue&);

        void addEvent(const Id<EventModel>&);
        bool removeEvent(const Id<EventModel>&);
        const QVector<Id<EventModel>>& events() const;
        void setEvents(const QVector<Id<EventModel>>& events);

        TriggerModel* trigger() const;

    signals:
        void extentChanged(const VerticalExtent&);
        void dateChanged(const TimeValue&);

        void newEvent(const Id<EventModel>& eventId);
        // TODO no eventRemoved ? eventsChanged ?

    private:
        VerticalExtent m_extent;
        TimeValue m_date{std::chrono::seconds{0}};
        TriggerModel* m_trigger;

        QVector<Id<EventModel>> m_events;
};
