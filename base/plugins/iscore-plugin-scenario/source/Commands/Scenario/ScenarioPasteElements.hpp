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
