#pragma once
#include "RecordData.hpp"

#include <Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Process/Temporal/StateMachines/ScenarioPoint.hpp>

#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <unordered_map>


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
