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
        iscore::ElementPluginModelList pluginModelList;

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
            checkIfPreviousConstraint();
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

        double y() const;
        void setY(double y);

        const QVector<id_type<EventModel>>& events() const;
        void setEvents(const QVector<id_type<EventModel>>& events);

        // TODO instead just prevent having reverse constraints...
        bool checkIfPreviousConstraint();

    public slots:
        void previousConstraintsChanged(const id_type<EventModel> &, bool);

    signals:
        void dateChanged();
        void newEvent(const id_type<EventModel>& eventId);
        void timeNodeValid(bool); // TODO wtf

    private:
        TimeValue m_date {std::chrono::seconds{0}};
        double m_y {0.0}; // TODO should not be necessary ?

        QVector<id_type<EventModel>> m_events;
        QMap<id_type<EventModel>, bool> m_eventHasPreviousConstraint;
};

