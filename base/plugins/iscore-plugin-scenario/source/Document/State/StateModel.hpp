#pragma once

#include <ProcessInterface/ModelMetadata.hpp>

#include <State/State.hpp>

#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/serialization/DataStreamVisitor.hpp>
#include <iscore/serialization/JSONVisitor.hpp>
#include <iscore/selection/Selectable.hpp>

#include <State/StateItemModel.hpp>

#include <DeviceExplorer/ItemModels/MessageItemModel.hpp>
#include <set>
#include "StateView.hpp"
#include "Document/Event/EventStatus.hpp"

class ConstraintView;
class ScenarioInterface;
class EventModel;
class ConstraintModel;
class Process;
class ProcessStateDataInterface;

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
            init();
        }

        const ScenarioInterface* parentScenario() const;

        double heightPercentage() const;

        const iscore::MessageItemModel &messages() const;
        iscore::MessageItemModel &messages();

        const Id<EventModel>& eventId() const;
        void setEventId(const Id<EventModel>&);

        const Id<ConstraintModel>& previousConstraint() const;
        const Id<ConstraintModel>& nextConstraint() const;

        // Note : the added constraint shall be in
        // the scenario when this is called.
        void setNextConstraint(const Id<ConstraintModel>&);
        void setPreviousConstraint(const Id<ConstraintModel>&);

        void setStatus(EventStatus);
        EventStatus status() const
        { return m_status; }

    signals:
        void sig_statesUpdated();
        void heightPercentageChanged();
        void statusChanged(EventStatus);

    public slots:
        void setHeightPercentage(double y);

    private:
        void statesUpdated_slt();
        void init(); // TODO check if other model elements need an init method too.
        void setConstraint_impl(const Id<ConstraintModel>& id);
        void on_nextProcessAdded(const Process&);
        void on_nextProcessRemoved(const Process&);
        void on_previousProcessAdded(const Process&);
        void on_previousProcessRemoved(const Process&);

        std::set<ProcessStateDataInterface*> m_previousProcesses;
        std::set<ProcessStateDataInterface*> m_nextProcesses;
        Id<EventModel> m_eventId;

        // OPTIMIZEME if we shift to Id = int, put this Optional
        Id<ConstraintModel> m_previousConstraint;
        Id<ConstraintModel> m_nextConstraint;

        double m_heightPercentage{0.5}; // In the whole scenario

        ptr<iscore::MessageItemModel> m_messageItemModel;
        EventStatus m_status{EventStatus::Editing};
};

