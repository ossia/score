#pragma once
#include <iscore/command/SerializableCommand.hpp>
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Process/Temporal/StateMachines/ScenarioPoint.hpp>

#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModelIdMap.hpp>
class TemporalScenarioLayerModel;
class ConstraintModel;
class EventModel;
class TimeNodeModel;
class StateModel;

class ScenarioPasteElements final : public iscore::SerializableCommand
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

        // TODO std::vector...
        QVector<Id<TimeNodeModel>> m_ids_timenodes;
        QVector<Id<ConstraintModel>> m_ids_constraints;
        QVector<Id<EventModel>> m_ids_events;
        QVector<Id<StateModel>> m_ids_states;

        QVector<QJsonObject> m_json_timenodes;
        QVector<QJsonObject> m_json_constraints;
        QVector<QJsonObject> m_json_events;
        QVector<QJsonObject> m_json_states;

        // TODO std::map...
        QMap<Id<ConstraintModel>, ConstraintViewModelIdMap> m_constraintViewModels;
};
