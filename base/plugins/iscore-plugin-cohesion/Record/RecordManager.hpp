#pragma once
#include "RecordData.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Scenario/Process/Temporal/StateMachines/ScenarioPoint.hpp>

#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <unordered_map>
#include <QTimer>

using RecordCommandDispatcher = GenericMacroCommandDispatcher<
    RedoStrategy::Quiet,
    SendStrategy::UndoRedo>;

// TODO for some reason we have to undo redo
// to be able to send the curve at execution. Investigate why.
class RecordManager final : public QObject
{
    public:
        RecordManager();

        void recordInNewBox(Scenario::ScenarioModel& scenar, Scenario::Point pt);
        // TODO : recordInExstingBox; recordFromState.
        void stopRecording();

        void commit();

    private:
        std::unique_ptr<RecordCommandDispatcher> m_dispatcher;
        ListeningState m_savedListening;
        std::vector<QMetaObject::Connection> m_recordCallbackConnections;

        DeviceExplorerModel* m_explorer{};

        QTimer m_recordTimer;
        std::chrono::steady_clock::time_point start_time_pt;

        std::unordered_map<
            iscore::Address,
            RecordData> records;
};
