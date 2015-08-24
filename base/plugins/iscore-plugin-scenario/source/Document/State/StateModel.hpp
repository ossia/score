#pragma once

#include <source/Document/ModelMetadata.hpp>

#include <State/State.hpp>

#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/selection/Selectable.hpp>

#include <State/StateItemModel.hpp>

#include "StateView.hpp"

class ConstraintView;
class ScenarioInterface;
class EventModel;
class ConstraintModel;

// Model for the graphical state in a scenario.
class StateModel : public IdentifiedObject<StateModel>
{
        Q_OBJECT

        ISCORE_SERIALIZE_FRIENDS(StateModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(StateModel, JSONObject)
    public:
        Selectable selection; // TODO state connections
        ModelMetadata metadata; // TODO : usefull ?

        StateModel(const id_type<StateModel>& id,
                   const id_type<EventModel>& eventId,
                   double yPos,
                   QObject* parent);

        // Copy
        StateModel(const StateModel& source,
                   const id_type<StateModel>&,
                   QObject* parent);

        // Load
        template<typename DeserializerVisitor,
                 enable_if_deserializer<DeserializerVisitor>* = nullptr>
        StateModel(DeserializerVisitor&& vis, QObject* parent) :
            IdentifiedObject{vis, parent}
        {
            vis.writeTo(*this);
        }

        const ScenarioInterface* parentScenario() const;

        double heightPercentage() const;

        const iscore::StateItemModel &states() const;
        iscore::StateItemModel &states();

        const id_type<EventModel>& eventId() const;

        const id_type<ConstraintModel>& previousConstraint() const;
        const id_type<ConstraintModel>& nextConstraint() const;
        void setNextConstraint(const id_type<ConstraintModel>&);
        void setPreviousConstraint(const id_type<ConstraintModel>&);

    signals:
        void statesUpdated();
        void heightPercentageChanged();

    public slots:
        void setHeightPercentage(double y);

    private:
        id_type<EventModel> m_eventId;  // TODO serialize

        // TODO When we shift to id_type = int, put this Optional
        id_type<ConstraintModel> m_previousConstraint;
        id_type<ConstraintModel> m_nextConstraint;

        double m_heightPercentage{0.5}; // In the whole scenario

        iscore::StateItemModel m_itemModel;
};

