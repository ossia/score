#pragma once
#include <score/command/AggregateCommand.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <memory>
#include <score/document/DocumentContext.hpp>
namespace Scenario { namespace Command {
class Macro
{
    RedoMacroCommandDispatcher<score::AggregateCommand> m;
  public:
    Macro(score::AggregateCommand* cmd, const score::DocumentContext& ctx);
    ~Macro();

    StateModel&
    createState(
        const Scenario::ProcessModel& scenar
        , const Id<EventModel>& ev
        , double y);

    std::tuple<TimeSyncModel&, EventModel&, StateModel&>
    createDot(
        const Scenario::ProcessModel& scenar
        , Scenario::Point pt);

    IntervalModel& createBox(
        const Scenario::ProcessModel& scenar
        , TimeVal start, TimeVal end, double y);

    IntervalModel& createIntervalAfter(
        const Scenario::ProcessModel& scenar
        , const Id<Scenario::StateModel>& state
        , Scenario::Point pt);

    IntervalModel& createInterval(
        const Scenario::ProcessModel& scenar
        , const Id<Scenario::StateModel>& start
        , const Id<Scenario::StateModel>& end);

    Process::ProcessModel* createProcess(
        const Scenario::IntervalModel& interval
        , const UuidKey<Process::ProcessModel>& key
        , const QString& data);

    template<typename T>
    T& createProcess(
        const Scenario::IntervalModel& interval
        , const QString& data)
    {
      return *safe_cast<T*>(
            this->createProcess(
              interval,
              Metadata<ConcreteKey_k, T>::get(),
              data));
    }

    Process::ProcessModel* createProcessInSlot(
        const Scenario::IntervalModel& interval
        , const UuidKey<Process::ProcessModel>& key
        , const QString& data);

    template<typename T>
    T& createProcessInSlot(
        const Scenario::IntervalModel& interval
        , const QString& data)
    {
      return *safe_cast<T*>(
            this->createProcessInSlot(
              interval,
              Metadata<ConcreteKey_k, T>::get(),
              data));
    }

    Process::ProcessModel* loadProcessInSlot(
        const Scenario::IntervalModel& interval
        , const QJsonObject& procdata);

    void clearInterval(
        const Scenario::IntervalModel&);

    void insertInInterval(
        QJsonObject&& sourceInterval,
        const IntervalModel& targetInterval,
        ExpandMode mode);

    void createSlot(
        const Scenario::IntervalModel& interval);

    void addLayer(
        const Scenario::IntervalModel& interval
        , int slot_index
        , const Process::ProcessModel& proc);

    void addLayerToLastSlot(
        const Scenario::IntervalModel& interval
        , const Process::ProcessModel& proc);

    void addLayerInNewSlot(
        const Scenario::IntervalModel& interval
        , const Process::ProcessModel& proc);

    void addLayer(
        const Scenario::SlotPath& slotpath
        , const Process::ProcessModel& proc);

    void showRack(
        const Scenario::IntervalModel& interval);

    void resizeSlot(
        const Scenario::IntervalModel& interval
        , const SlotPath& slotPath
        , double newSize);

    void resizeSlot(
        const Scenario::IntervalModel& interval
        , SlotPath&& slotPath
        , double newSize);

    IntervalModel& duplicate(
        const Scenario::ProcessModel& scenario
        , const Scenario::IntervalModel& itv);

    Process::ProcessModel& duplicateProcess(
        const Scenario::IntervalModel& itv,
        const Process::ProcessModel& process);

    void pasteElements(
        const Scenario::ProcessModel& scenario
        , const QJsonObject& objs
        , Scenario::Point pos);

    void mergeTimeSyncs(
        const Scenario::ProcessModel& scenario
        , const Id<TimeSyncModel>& a
        , const Id<TimeSyncModel>& b);

    void removeProcess(
        const Scenario::IntervalModel& interval
        , const Id<Process::ProcessModel>& proc);

    void removeElements(
        const Scenario::ProcessModel& scenario
        , const Selection& sel);

    void addMessages(
        const Scenario::StateModel& state
        , State::MessageList msgs);

    void submit(score::Command* cmd);
    void commit();
};
} }
