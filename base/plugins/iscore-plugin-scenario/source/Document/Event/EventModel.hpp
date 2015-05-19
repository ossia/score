#pragma once
#include <source/Document/ModelMetadata.hpp>

#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/selection/Selectable.hpp>
#include <ProcessInterface/TimeValue.hpp>

#include <unordered_map>
#include <State/State.hpp>

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
        Q_PROPERTY(double heightPercentage
                   READ heightPercentage
                   WRITE setHeightPercentage
                   NOTIFY heightPercentageChanged)

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

        static QString prettyName();

        ScenarioModel* parentScenario() const;

        /** The class **/
        EventModel(const id_type<EventModel>&,
                   const id_type<TimeNodeModel>& timenode,
                   double yPos,
                   QObject* parent);

        // Copy
        EventModel(const EventModel* source,
                   const id_type<EventModel>&,
                   QObject* parent);

        template<typename DeserializerVisitor>
        EventModel(DeserializerVisitor&& vis, QObject* parent) :
            IdentifiedObject<EventModel> {vis, parent}
        {
            vis.writeTo(*this);
        }

        // Constraints
        const QVector<id_type<ConstraintModel>>& previousConstraints() const;
        const QVector<id_type<ConstraintModel>>& nextConstraints() const;
        QVector<id_type<ConstraintModel> > constraints() const;

        void addNextConstraint(const id_type<ConstraintModel>&);
        void addPreviousConstraint(const id_type<ConstraintModel>&);
        bool removeNextConstraint(const id_type<ConstraintModel>&);
        bool removePreviousConstraint(const id_type<ConstraintModel>&);

        // Timenode
        void changeTimeNode(const id_type<TimeNodeModel>&);
        const id_type<TimeNodeModel>& timeNode() const;

        // States
        const StateList& states() const;
        void replaceStates(const StateList& newStates);
        void addState(const State& s);
        void removeState(const State& s);

        // Other properties
        double heightPercentage() const;

        const TimeValue& date() const;
        void translate(const TimeValue& deltaTime);

        // TODO use a stronger type for the condition.
        const QString& condition() const;
        QString trigger() const;


        auto& pluginModelList() { return *m_pluginModelList; }
        const auto& pluginModelList() const { return *m_pluginModelList; }



    public slots:
        void setHeightPercentage(double arg);
        void setDate(const TimeValue& date);

        void setCondition(const QString& arg);
        void setTrigger(QString trigger);

    signals:
        void selectionChanged(bool);
        void heightPercentageChanged(double arg);
        void messagesChanged();
        void dateChanged();

        void conditionChanged(const QString& arg);
        void triggerChanged(QString trigger);

    private:
        iscore::ElementPluginModelList* m_pluginModelList{};
        id_type<TimeNodeModel> m_timeNode {};

        QVector<id_type<ConstraintModel>> m_previousConstraints;
        QVector<id_type<ConstraintModel>> m_nextConstraints;

        double m_heightPercentage {0.5};

        StateList m_states;
        QString m_condition {};

        TimeValue m_date {std::chrono::seconds{0}};
        QString m_trigger;
};
