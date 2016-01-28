#pragma once
#include <Scenario/Commands/ScenarioCommandFactory.hpp>
#include <Scenario/Document/Constraint/ViewModels/ConstraintViewModelIdMap.hpp>
#include <boost/optional/optional.hpp>
#include <iscore/command/SerializableCommand.hpp>
#include <QJsonObject>
#include <QMap>
#include <QVector>

#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>

class DataStreamInput;
class DataStreamOutput;

namespace Scenario
{
struct Point;
class EventModel;
class StateModel;
class TemporalScenarioLayerModel;
class TimeNodeModel;
class ConstraintModel;
namespace Command
{

class ScenarioPasteElements final : public iscore::SerializableCommand
{
        ISCORE_COMMAND_DECL(ScenarioCommandFactoryName(), ScenarioPasteElements, "Paste elements in scenario")
    public:
        ScenarioPasteElements(
                Path<TemporalScenarioLayerModel>&& path,
                const QJsonObject& obj,
                const Scenario::Point& pt);

        void undo() const override;
        void redo() const override;

    protected:
        void serializeImpl(DataStreamInput&) const override;
        void deserializeImpl(DataStreamOutput&) override;

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
}
}
