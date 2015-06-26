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
        friend void Visitor<Reader<JSONObject>>::readFrom<TimeNodeModel> (const TimeNodeModel& ev);
        friend void Visitor<Writer<DataStream>>::writeTo<TimeNodeModel> (TimeNodeModel& ev);
        friend void Visitor<Writer<JSONObject>>::writeTo<TimeNodeModel> (TimeNodeModel& ev);

    public:
        /** Properties of the class **/
        Selectable selection;
        ModelMetadata metadata;

        static QString prettyName()
        { return QObject::tr("Time Node"); }


        /** The class **/
        TimeNodeModel(
                const id_type<TimeNodeModel>& id,
                const TimeValue& date,
                double ypos,
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
        ScenarioModel* parentScenario() const;

        // Data of the TimeNode
        void addEvent(const id_type<EventModel>&);
        bool removeEvent(const id_type<EventModel>&);

        const TimeValue& date() const;
        void setDate(const TimeValue&);

        bool isEmpty();

        double y() const;
        void setY(double y);

        const QVector<id_type<EventModel>>& events() const;
        void setEvents(const QVector<id_type<EventModel>>& events);

        auto& pluginModelList() { return *m_pluginModelList; }

        bool checkIfPreviousConstraint();

    public slots:
        void previousConstraintsChanged(const id_type<EventModel> &, bool);

    signals:
        void dateChanged();
        void newEvent(const id_type<EventModel>& eventId);
        void timeNodeValid(bool);

    private:
        iscore::ElementPluginModelList* m_pluginModelList{};
        TimeValue m_date {std::chrono::seconds{0}};
        double m_y {0.0};

        QVector<id_type<EventModel>> m_events;
        QMap<id_type<EventModel>, bool> m_eventHasPreviousConstraint;
};

