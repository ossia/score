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
        ISCORE_METADATA("StateModel")

        ISCORE_SERIALIZE_FRIENDS(StateModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(StateModel, JSONObject)
    public:
        Selectable selection;
        ModelMetadata metadata;

        StateModel(const Id<StateModel>& id,
                   const Id<EventModel>& eventId,
                   double yPos,
                   QObject* parent);

        // Copy
        StateModel(const StateModel& source,
                   const Id<StateModel>&,
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

        const Id<EventModel>& eventId() const;
        void setEventId(const Id<EventModel>&);

        const Id<ConstraintModel>& previousConstraint() const;
        const Id<ConstraintModel>& nextConstraint() const;
        void setNextConstraint(const Id<ConstraintModel>&);
        void setPreviousConstraint(const Id<ConstraintModel>&);

    signals:
        void statesUpdated();
        void heightPercentageChanged();

    public slots:
        void setHeightPercentage(double y);

    private:
        Id<EventModel> m_eventId;

        // OPTIMIZEME if we shift to Id = int, put this Optional
        Id<ConstraintModel> m_previousConstraint;
        Id<ConstraintModel> m_nextConstraint;

        double m_heightPercentage{0.5}; // In the whole scenario

        iscore::StateItemModel m_itemModel;
};

