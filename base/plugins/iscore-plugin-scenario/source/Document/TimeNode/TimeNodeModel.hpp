#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <source/Document/VerticalExtent.hpp>
#include <source/Document/ModelMetadata.hpp>
#include <ProcessInterface/TimeValue.hpp>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/selection/Selectable.hpp>

#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>
class EventModel;
class ScenarioInterface;
class TimeNodeModel : public IdentifiedObject<TimeNodeModel>
{
        Q_OBJECT

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
                const id_type<TimeNodeModel>& id,
                const VerticalExtent& extent,
                const TimeValue& date,
                QObject* parent);

        template<typename DeserializerVisitor>
        TimeNodeModel(DeserializerVisitor&& vis, QObject* parent) :
            IdentifiedObject<TimeNodeModel> {vis, parent}
        {
            vis.writeTo(*this);
        }

        TimeNodeModel(
                const TimeNodeModel& source,
                const id_type<TimeNodeModel>& id,
                QObject* parent);


        // Utility
        ScenarioInterface* parentScenario() const;

        // Data of the TimeNode
        const VerticalExtent& extent() const;
        void setExtent(const VerticalExtent &extent);

        const TimeValue& date() const;
        void setDate(const TimeValue&);

        void addEvent(const id_type<EventModel>&);
        bool removeEvent(const id_type<EventModel>&);
        const QVector<id_type<EventModel>>& events() const;
        void setEvents(const QVector<id_type<EventModel>>& events);

    signals:
        void extentChanged(const VerticalExtent&);
        void dateChanged(const TimeValue&);

        void newEvent(const id_type<EventModel>& eventId);
        // TODO no eventRemoved ? eventsChanged ?

    private:
        VerticalExtent m_extent;
        TimeValue m_date{std::chrono::seconds{0}};

        QVector<id_type<EventModel>> m_events;
};

