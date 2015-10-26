#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <Commands/ScenarioCommandFactory.hpp>
#include <Process/Temporal/StateMachines/ScenarioPoint.hpp>

#include <Document/Constraint/ViewModels/ConstraintViewModelIdMap.hpp>
class TemporalScenarioLayerModel;
class ConstraintModel;
class EventModel;
class TimeNodeModel;
class StateModel;

// TODO add me to command lists
class ScenarioPasteElements : public iscore::SerializableCommand
{
        ISCORE_SERIALIZABLE_COMMAND_DECL(ScenarioCommandFactoryName(),
                                         ScenarioPasteElements,
                                         "ScenarioPasteElements")
    public:
        ScenarioPasteElements(
                Path<TemporalScenarioLayerModel>&& path,
                const QJsonObject& obj,
                const ScenarioPoint& pt);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(QDataStream&) const override;
        void deserializeImpl(QDataStream&) override;

    private:
        Path<TemporalScenarioLayerModel> m_ts;
        std::vector<TimeNodeModel*> m_timenodes;
        std::vector<ConstraintModel*> m_constraints;
        std::vector<EventModel*> m_events;
        std::vector<StateModel*> m_states;
/*
        std::vector<QJsonObject> m_json_timenodes;
        std::vector<QJsonObject> m_json_constraints;
        std::vector<QJsonObject> m_json_events;
        std::vector<QJsonObject> m_json_states;
*/
        std::map<Id<ConstraintModel>, ConstraintViewModelIdMap> m_constraintViewModels;
};
