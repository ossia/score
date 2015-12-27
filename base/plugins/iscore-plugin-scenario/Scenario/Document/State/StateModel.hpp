#pragma once
#include <iscore/tools/Metadata.hpp>
#include <Process/ModelMetadata.hpp>
#include <Process/StateProcess.hpp>
#include <Scenario/Document/Event/ExecutionStatus.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/selection/Selectable.hpp>
#include <iscore/tools/IdentifiedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <nano_signal_slot.hpp>

#include <set>
#include <vector>

#include <iscore/serialization/VisitorInterface.hpp>
#include <iscore/tools/Todo.hpp>
#include <iscore_plugin_scenario_export.h>
#include <iscore/component/Component.hpp>
class ConstraintModel;
class DataStream;
class EventModel;
class JSONObject;
class MessageItemModel;
class Process;
class ProcessStateDataInterface;

namespace iscore
{
class CommandStackFacade;
}

class ProcessStateWrapper : public QObject
{
    private:
        ProcessStateDataInterface* m_proc;

    public:
        ProcessStateWrapper(ProcessStateDataInterface* proc):
            m_proc{proc}
        {

        }

        ProcessStateDataInterface& process() const { return *m_proc; }
};

// Model for the graphical state in a scenario.
class ISCORE_PLUGIN_SCENARIO_EXPORT StateModel final : public IdentifiedObject<StateModel>, public Nano::Observer
{
        Q_OBJECT
        ISCORE_METADATA(StateModel)

        ISCORE_SERIALIZE_FRIENDS(StateModel, DataStream)
        ISCORE_SERIALIZE_FRIENDS(StateModel, JSONObject)
    public:
        using ProcessVector = std::list<ProcessStateWrapper>;
        iscore::Components components;
        Selectable selection;
        ModelMetadata metadata;

        StateModel(const Id<StateModel>& id,
                   const Id<EventModel>& eventId,
                   double yPos,
                   iscore::CommandStackFacade& stack,
                   QObject* parent);

        // Copy
        StateModel(const StateModel& source,
                   const Id<StateModel>&,
                   iscore::CommandStackFacade& stack,
                   QObject* parent);

        // Load
        template<typename DeserializerVisitor,
                 enable_if_deserializer<DeserializerVisitor>* = nullptr>
        StateModel(DeserializerVisitor&& vis,
                   iscore::CommandStackFacade& stack,
                   QObject* parent) :
            IdentifiedObject{vis, parent},
            m_stack{stack}
        {
            vis.writeTo(*this);
            init();
        }

        static QString description()
        { return "State"; }
        double heightPercentage() const;

        MessageItemModel &messages() const;

        const Id<EventModel>& eventId() const;
        void setEventId(const Id<EventModel>&);

        const Id<ConstraintModel>& previousConstraint() const;
        const Id<ConstraintModel>& nextConstraint() const;

        // Note : the added constraint shall be in
        // the scenario when this is called.
        void setNextConstraint(const Id<ConstraintModel>&);
        void setPreviousConstraint(const Id<ConstraintModel>&);

        ProcessVector& previousProcesses()
        { return m_previousProcesses; }
        ProcessVector& followingProcesses()
        { return m_nextProcesses; }
        const ProcessVector& previousProcesses() const
        { return m_previousProcesses; }
        const ProcessVector& followingProcesses() const
        { return m_nextProcesses; }

        void setStatus(ExecutionStatus);
        ExecutionStatus status() const
        { return m_status.get(); }

        NotifyingMap<StateProcess> stateProcesses;

        void setHeightPercentage(double y);

    signals:
        void sig_statesUpdated();
        void heightPercentageChanged();
        void statusChanged(ExecutionStatus);

    private:
        void statesUpdated_slt();
        void init(); // TODO check if other model elements need an init method too.
        iscore::CommandStackFacade& m_stack;

        ProcessVector m_previousProcesses;
        ProcessVector m_nextProcesses;
        Id<EventModel> m_eventId;

        // OPTIMIZEME if we shift to Id = int, put this Optional
        Id<ConstraintModel> m_previousConstraint;
        Id<ConstraintModel> m_nextConstraint;

        double m_heightPercentage{0.5}; // In the whole scenario

        ptr<MessageItemModel> m_messageItemModel;
        ExecutionStatusProperty m_status{};
};
