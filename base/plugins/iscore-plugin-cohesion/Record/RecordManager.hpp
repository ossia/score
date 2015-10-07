#pragma once
#include "RecordData.hpp"

#include <Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Process/Temporal/StateMachines/ScenarioPoint.hpp>

#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <unordered_map>


using RecordCommandDispatcher = GenericMacroCommandDispatcher<
    RedoStrategy::Quiet,
    SendStrategy::UndoRedo>;

// TODO for some reason we have to undo redo
// to be able to send the curve at execution. Investigate why.
class RecordManager : public QObject
{
    public:
        RecordManager();

        void recordInNewBox(ScenarioModel& scenar, ScenarioPoint pt);
        // TODO : recordInExstingBox; recordFromState.
        void stopRecording();

        void commit();

    private:
        std::unique_ptr<RecordCommandDispatcher> m_dispatcher;
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
