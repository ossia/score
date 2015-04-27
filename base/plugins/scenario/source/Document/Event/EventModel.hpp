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

        friend void Visitor<Reader<DataStream>>::readFrom<EventModel> (const EventModel& ev);
        friend void Visitor<Reader<JSONObject>>::readFrom<EventModel> (const EventModel& ev);
        friend void Visitor<Writer<DataStream>>::writeTo<EventModel> (EventModel& ev);
        friend void Visitor<Writer<JSONObject>>::writeTo<EventModel> (EventModel& ev);

    public:
        /** Public properties of the class **/
        Selectable selection;
        ModelMetadata metadata;

        static QString prettyName()
        { return QObject::tr("Event"); }

        ScenarioModel* parentScenario() const;

        /** The class **/
        EventModel(id_type<EventModel>,
                   id_type<TimeNodeModel> timenode,
                   double yPos,
                   QObject* parent);

        // Copy
        EventModel(EventModel* source,
                   id_type<EventModel>,
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
        QVector<id_type<ConstraintModel> > constraints();

        void addNextConstraint(id_type<ConstraintModel>);
        void addPreviousConstraint(id_type<ConstraintModel>);
        bool removeNextConstraint(id_type<ConstraintModel>);
        bool removePreviousConstraint(id_type<ConstraintModel>);

        // Timenode
        void changeTimeNode(id_type<TimeNodeModel>);
        id_type<TimeNodeModel> timeNode() const;

        // States
        const StateList& states() const;
        void replaceStates(StateList newStates);
        void addState(const State& s);
        void removeState(const State& s);

        // Other properties
        double heightPercentage() const;

        TimeValue date() const;
        void translate(const TimeValue& deltaTime);

        // TODO use a stronger type for the condition.
        QString condition() const;

        auto& pluginModelList() { return *m_pluginModelList; }

    public slots:
        void setHeightPercentage(double arg);
        void setDate(const TimeValue& date);
        void setCondition(const QString& arg);

    signals:
        void selectionChanged(bool);
        void heightPercentageChanged(double arg);
        void messagesChanged();
        void conditionChanged(QString arg);
        void dateChanged();

    private:
        iscore::ElementPluginModelList* m_pluginModelList{};
        id_type<TimeNodeModel> m_timeNode {};

        QVector<id_type<ConstraintModel>> m_previousConstraints;
        QVector<id_type<ConstraintModel>> m_nextConstraints;

        double m_heightPercentage {0.5};

        StateList m_states;
        QString m_condition {};

        TimeValue m_date {std::chrono::seconds{0}};
};
