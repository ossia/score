#pragma once

class TemporalScenarioPresenter;

namespace iscore
{
    class SerializableCommand;
}

struct EventData;
struct ConstraintData;

class ScenarioCommandManager
{
    public:
        ScenarioCommandManager(TemporalScenarioPresenter *presenter);

        void createConstraint(EventData);

        void clearContentFromSelection();
        void deleteSelection();

        // Moving
        void moveEventAndConstraint(EventData data);
        void moveConstraint(ConstraintData data);
        void moveTimeNode(EventData data);

        // Helpers
        void sendOngoingCommand(iscore::SerializableCommand* cmd);
        void finishOngoingCommand();
        void rollbackOngoingCommand();

        void on_ctrlStateChanged(bool);

        // Necessary for the real-time creation / moving of elements
        bool m_ongoingCommand{};
        int m_ongoingCommandId{-1};
    private:

        TemporalScenarioPresenter* m_presenter;


};
