#include <iscore/command/SerializableCommand.hpp>
#include <Commands/Scenario/Displacement/MoveEvent.hpp>

class MoveEventMeta : iscore::SerializableCommand
{
    ISCORE_COMMAND_DECL("ScenarioControl", "move", "move")
    // Command interface
    public:
        ISCORE_SERIALIZABLE_COMMAND_DEFAULT_CTOR(MoveEventMeta)
      MoveEventMeta(
      Path<ScenarioModel>&& scenarioPath,
        const Id<EventModel>& eventId,
              const TimeValue& newDate,
              ExpandMode mode);

    void undo();
    void redo();

    // SerializableCommand interface
protected:
    void serializeImpl(QDataStream&) const;
    void deserializeImpl(QDataStream&);

private:
    iscore::SerializableCommand* m_moveEventImplementation;
};
