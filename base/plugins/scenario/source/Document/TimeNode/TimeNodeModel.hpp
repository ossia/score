#pragma once
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <source/Document/ModelMetadata.hpp>
#include <ProcessInterface/TimeValue.hpp>

#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/selection/Selectable.hpp>

#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>
class EventModel;
class ScenarioModel;
class TimeNodeModel : public IdentifiedObject<TimeNodeModel>
{
        Q_OBJECT

        friend void Visitor<Reader<DataStream>>::readFrom<TimeNodeModel> (const TimeNodeModel& ev);
        friend void Visitor<Reader<JSON>>::readFrom<TimeNodeModel> (const TimeNodeModel& ev);
        friend void Visitor<Writer<DataStream>>::writeTo<TimeNodeModel> (TimeNodeModel& ev);
        friend void Visitor<Writer<JSON>>::writeTo<TimeNodeModel> (TimeNodeModel& ev);

    public:
        /** Properties of the class **/
        Selectable selection;
        ModelMetadata metadata;

        static QString prettyName()
        { return QObject::tr("Time Node"); }


        /** The class **/
        TimeNodeModel(id_type<TimeNodeModel> id, TimeValue date, double ypos, QObject* parent);

        template<typename DeserializerVisitor>
        TimeNodeModel(DeserializerVisitor&& vis, QObject* parent) :
            IdentifiedObject<TimeNodeModel> {vis, parent}
        {
            vis.writeTo(*this);
        }

        TimeNodeModel(TimeNodeModel* source, id_type<TimeNodeModel> id, QObject* parent):
            TimeNodeModel{id, source->date(), source->y(), parent}
        {
            m_pluginModelList = new iscore::ElementPluginModelList{source->m_pluginModelList, this};
            m_events = source->m_events;
        }


        // Utility
        ScenarioModel* parentScenario() const;

        // Data of the TimeNode
        void addEvent(id_type<EventModel>);
        bool removeEvent(id_type<EventModel>);

        TimeValue date() const;

        void setDate(TimeValue);

        bool isEmpty();

        double y() const;
        void setY(double y);

        QVector<id_type<EventModel>> events() const;
        void setEvents(const QVector<id_type<EventModel>>& events);

        auto& pluginModelList() { return *m_pluginModelList; }

    signals:
        void dateChanged();
        void newEvent(id_type<EventModel> eventId);

    private:
        iscore::ElementPluginModelList* m_pluginModelList{};
        TimeValue m_date {std::chrono::seconds{0}};
        double m_y {0.0};

        QVector<id_type<EventModel>> m_events;
};

