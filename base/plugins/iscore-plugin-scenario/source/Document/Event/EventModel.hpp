#pragma once
#include <source/Document/ModelMetadata.hpp>
#include <source/Document/VerticalExtent.hpp>

#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/selection/Selectable.hpp>
#include <ProcessInterface/TimeValue.hpp>

#include <unordered_map>
#include <State/State.hpp>
#include "source/Document/State/DisplayedStateModel.hpp"

#include <iscore/plugins/documentdelegate/plugin/ElementPluginModelList.hpp>
namespace OSSIA
{
    class TimeNode;
}
class ConstraintModel;
class TimeNodeModel;
class ScenarioModel;

class EventModel : public IdentifiedObject<EventModel>
{
        Q_OBJECT

    private:
        Q_PROPERTY(QString condition
                   READ condition
                   WRITE setCondition
                   NOTIFY conditionChanged)

        Q_PROPERTY(QString trigger
                   READ trigger
                   WRITE setTrigger
                   NOTIFY triggerChanged)

        friend void Visitor<Reader<DataStream>>::readFrom<EventModel> (const EventModel& ev);
        friend void Visitor<Reader<JSONObject>>::readFrom<EventModel> (const EventModel& ev);
        friend void Visitor<Writer<DataStream>>::writeTo<EventModel> (EventModel& ev);
        friend void Visitor<Writer<JSONObject>>::writeTo<EventModel> (EventModel& ev);

    public:
        /** Public properties of the class **/
        Selectable selection;
        ModelMetadata metadata;
        iscore::ElementPluginModelList pluginModelList;

        static QString prettyName();

        ScenarioModel* parentScenario() const;

        /** The class **/
        EventModel(const id_type<EventModel>&,
                   const id_type<TimeNodeModel>& timenode,
                   const VerticalExtent& extent,
                   const TimeValue& date,
                   QObject* parent);

        // Copy
        EventModel(const EventModel& source,
                   const id_type<EventModel>&,
                   QObject* parent);

        template<typename DeserializerVisitor>
        EventModel(DeserializerVisitor&& vis, QObject* parent) :
            IdentifiedObject<EventModel> {vis, parent}
        {
            vis.writeTo(*this);
        }

        // Timenode
        void changeTimeNode(const id_type<TimeNodeModel>&);
        const id_type<TimeNodeModel>& timeNode() const;

        // States
        void addDisplayedState(const id_type<StateModel>& ds);
        void removeDisplayedState(const id_type<StateModel>& ds);

        // Other properties
        // TODO use a stronger type for the condition.
        const QString& condition() const;
        const QString& trigger() const;

        VerticalExtent extent() const;

        const TimeValue& date() const;
        void translate(const TimeValue& deltaTime);


    public slots:
        void setCondition(const QString& arg);
        void setTrigger(const QString& trigger);

        void setExtent(const VerticalExtent &extent);
        void setDate(const TimeValue& date);

    signals:
        void extentChanged(const VerticalExtent&);
        void dateChanged(const TimeValue&);

        void conditionChanged(const QString&);
        void triggerChanged(const QString&);

        void statesChanged();

    private:
        id_type<TimeNodeModel> m_timeNode {};

        QVector<id_type<StateModel>> m_states;

        QString m_condition;
        QString m_trigger; // TODO in timenode?

        VerticalExtent m_extent;
        TimeValue m_date{TimeValue::zero()};
};
