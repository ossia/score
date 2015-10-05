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
            m_cmds[1]->undo();
            m_cmds[0]->undo();
        }
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
class DeviceExplorerModel;

struct RecordData
{
        RecordData(
                Scenario::Command::AddProcessToConstraint* cmd,
                CurveModel& cm,
                PointArrayCurveSegmentModel& seg):
            addProcCmd{cmd},
            curveModel{cm},
            segment{seg}
        { }

        Scenario::Command::AddProcessToConstraint* addProcCmd{};

        CurveModel& curveModel;
        PointArrayCurveSegmentModel& segment;
};
class RecordManager : public QObject
{
    public:
        RecordManager();

        void recordInNewBox(ScenarioModel& scenar, ScenarioPoint pt);
        // TODO : recordInExstingBox; recordFromState.
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
