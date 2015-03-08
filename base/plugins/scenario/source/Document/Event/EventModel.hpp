#pragma once
#include <public_interface/tools/IdentifiedObject.hpp>
#include <public_interface/tools/SettableIdentifier.hpp>
#include <public_interface/serialization/DataStreamVisitor.hpp>
#include <public_interface/serialization/JSONVisitor.hpp>
#include "Document/ModelMetadata.hpp"
#include <public_interface/selection/Selectable.hpp>
#include <ProcessInterface/TimeValue.hpp>

#include <unordered_map>
namespace OSSIA
{
    class TimeNode;
}
class State;
class ConstraintModel;
class TimeNodeModel;
class ScenarioModel;

class EventModel : public IdentifiedObject<EventModel>
{
        Q_OBJECT

        Q_PROPERTY(double heightPercentage
                   READ heightPercentage
                   WRITE setHeightPercentage
                   NOTIFY heightPercentageChanged)

        Q_PROPERTY(QString condition
                   READ condition
                   WRITE setCondition
                   NOTIFY conditionChanged)

        friend void Visitor<Writer<DataStream>>::writeTo<EventModel> (EventModel& ev);
        friend void Visitor<Writer<JSON>>::writeTo<EventModel> (EventModel& ev);

    public:
        /** Public properties of the class **/
        Selectable selection;
        ModelMetadata metadata;

        static constexpr const char * className()
        { return "EventModel"; }
        static QString prettyName()
        { return QObject::tr("Event"); }


        /** The class **/
        EventModel(id_type<EventModel>, QObject* parent);
        EventModel(id_type<EventModel>, double yPos, QObject* parent);


        // Copy
        EventModel(EventModel* source,
                   id_type<EventModel>,
                   QObject* parent);
        ~EventModel();

        template<typename DeserializerVisitor>
        EventModel(DeserializerVisitor&& vis, QObject* parent) :
            IdentifiedObject<EventModel> {vis, parent}
        {
            vis.writeTo(*this);
        }

        const QVector<id_type<ConstraintModel>>& previousConstraints() const;
        const QVector<id_type<ConstraintModel>>& nextConstraints() const;

        void addNextConstraint(id_type<ConstraintModel>);
        void addPreviousConstraint(id_type<ConstraintModel>);

        bool removeNextConstraint(id_type<ConstraintModel>);
        bool removePreviousConstraint(id_type<ConstraintModel>);

        void changeTimeNode(id_type<TimeNodeModel>);
        id_type<TimeNodeModel> timeNode() const;

        const std::vector<State*>& states() const;
        void addState(State* s);
        void removeState(id_type<State> stateId);

        OSSIA::TimeNode* apiObject()
        {
            return m_timeEvent;
        }

        double heightPercentage() const;
        TimeValue date() const;

        void translate(TimeValue deltaTime);

        void setVerticalExtremity(id_type<ConstraintModel>, double);
        void eventMovedVertically(double);

        ScenarioModel* parentScenario() const;

        // Should maybe be in the Scenario instead ?
        QMap<id_type<ConstraintModel>, double> constraintsYPos() const
        {
            return m_constraintsYPos;
        }
        double topY() const
        {
            return m_topY;
        }
        double bottomY() const
        {
            return m_bottomY;
        }

        QString condition() const;

    public slots:
        void setHeightPercentage(double arg);
        void setDate(TimeValue date);
        void setCondition(QString arg);

    signals:
        void selectionChanged(bool);
        void heightPercentageChanged(double arg);
        void messagesChanged();
        void conditionChanged(QString arg);

    private:
        void setTopY(double val);
        void setBottomY(double val);

        void setOSSIATimeNode(OSSIA::TimeNode* timeEvent)
        {
            m_timeEvent = timeEvent;
        }


        OSSIA::TimeNode* m_timeEvent {};

        id_type<TimeNodeModel> m_timeNode {};

        QVector<id_type<ConstraintModel>> m_previousConstraints;
        QVector<id_type<ConstraintModel>> m_nextConstraints;
        QMap<id_type<ConstraintModel>, double> m_constraintsYPos;

        double m_heightPercentage {0.5};

        double m_topY {0.5};
        double m_bottomY {0.5};

        std::vector<State*> m_states;
        QString m_condition {};

        /// TEMPORARY. This information has to be queried from OSSIA::Scenario instead.
        TimeValue m_date {std::chrono::seconds{0}}; // Was : m_x
};
