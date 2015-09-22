#pragma once
#include <unordered_map>
#include <iscore/command/AggregateCommand.hpp>
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>

#include "Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp"
#include "Process/Temporal/StateMachines/ScenarioPoint.hpp"

// MOVEME
class Record : public iscore::AggregateCommand
{
         ISCORE_COMMAND_DECL("IScoreCohesionControl", "Record", "Record")
    public:
        Record():
            AggregateCommand{factoryName(),
                             commandName(),
                             description()}
        {

        }

        void undo() override
        {
            ISCORE_TODO;
        }

// TODO I require a special undo too
};

namespace Scenario
{
namespace Command
{
class AddProcessToConstraint;
}
}
class CurveModel;
class PointArrayCurveSegmentModel;
class ScenarioModel;
struct RecordData
{
        Scenario::Command::AddProcessToConstraint* addProcCmd{};

        CurveModel& curveModel;
        PointArrayCurveSegmentModel& segment;
        double min{};
        double max{};
        double initVal{};
};

class DeviceExplorerModel;
class RecordManager : public QObject
{
    public:
        RecordManager();

        void initRecording(ScenarioModel& scenar, ScenarioPoint pt);
        void stopRecording();

        void commit();

    private:
        std::unique_ptr<QuietMacroCommandDispatcher> m_dispatcher;
        ListeningState m_savedListening;
        std::vector<std::vector<iscore::Address>> m_recordListening;
        std::vector<QMetaObject::Connection> m_recordCallbackConnections;

        DeviceExplorerModel* m_explorer{};

        QTimer m_recordTimer;
        std::chrono::steady_clock::time_point start_time_pt;

        std::unordered_map<
            iscore::Address,
            RecordData> records;
};
