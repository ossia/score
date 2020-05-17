// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "RecordMessagesManager.hpp"

#include <Device/Address/AddressSettings.hpp>
#include <Device/Address/IOType.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceInterface.hpp>
#include <Explorer/DeviceList.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/Explorer/ListeningManager.hpp>
#include <Process/ExpandMode.hpp>
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include <Recording/Commands/Record.hpp>
#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Commands/Interval/Rack/Slot/AddLayerModelToSlot.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveNewEvent.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveNewState.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Slot.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>

#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/tools/std/Optional.hpp>

#include <core/document/Document.hpp>

#include <QApplication>
#include <qnamespace.h>

#include <wobjectimpl.h>

#include <utility>

#include <type_traits>
W_OBJECT_IMPL(Recording::MessageRecorder)
namespace Recording
{
MessageRecorder::MessageRecorder(RecordContext& ctx) : context{ctx} { }

void MessageRecorder::stop()
{
  // Stop all the recording machinery
  for (const auto& dev : m_recordCallbackConnections)
  {
    if (dev)
      (*dev).valueUpdated.disconnect<&MessageRecorder::on_valueUpdated>(*this);
  }
  m_recordCallbackConnections.clear();

  QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

  // Record and then stop
  if (!context.started())
  {
    context.dispatcher.rollback();
    return;
  }

  auto& states = m_createdProcess->startEvent().states();
  SCORE_ASSERT(!states.empty());
  Id<Scenario::StateModel> startState = states.front();
  auto setStateCmd = new Scenario::Command::AddMessagesToState(
      m_createdProcess->state(startState), {m_records[0].m});
  setStateCmd->redo(context.context);
  context.dispatcher.submit(setStateCmd);

  auto movecmd = new Scenario::Command::MoveNewState{*m_createdProcess, startState, 0.5};
  movecmd->redo(context.context);
  context.dispatcher.submit(movecmd);

  for (std::size_t i = 1; i < m_records.size(); i++)
  {
    RecordedMessage& val = m_records[i];

    // Create a state
    auto cmd = new Scenario::Command::CreateInterval_State_Event_TimeSync{
        *m_createdProcess, startState, TimeVal::fromMsecs(val.percentage), 0.5, false};
    cmd->redo(context.context);
    startState = cmd->createdState();
    context.dispatcher.submit(cmd);

    // Add messages to it
    auto setStateCmd
        = new Scenario::Command::AddMessagesToState(m_createdProcess->state(startState), {val.m});
    setStateCmd->redo(context.context);
    context.dispatcher.submit(setStateCmd);
  }
}

void MessageRecorder::on_valueUpdated(const State::Address& addr, const ossia::value& val)
{
  if (context.started())
  {
    // Move end event by the current duration.
    auto msecs = context.timeInDouble();

    m_records.push_back(RecordedMessage{msecs, State::Message{State::AddressAccessor{addr}, val}});

    m_createdProcess->setDuration(TimeVal::fromMsecs(msecs));
  }
  else
  {
    firstMessageReceived();
    context.start();

    m_records.push_back(RecordedMessage{0., State::Message{State::AddressAccessor{addr}, val}});
  }
}

bool MessageRecorder::setup(const Box& box, const RecordListening& recordListening)
{
  using namespace std::chrono;
  //// Device tree management ////

  //// Creation of the process ////
  // Note : since we directly create the IDs here, we don't have to worry
  // about their generation.
  auto cmd_proc = new Scenario::Command::AddOnlyProcessToInterval{
      box.interval, Metadata<ConcreteKey_k, Scenario::ProcessModel>::get(), {}, {}};

  cmd_proc->redo(context.context);
  context.dispatcher.submit(cmd_proc);

  auto& proc = box.interval.processes.at(cmd_proc->processId());
  auto& record_proc = static_cast<Scenario::ProcessModel&>(proc);
  m_createdProcess = &record_proc;

  //// Creation of the layer ////
  auto cmd_layer = new Scenario::Command::AddLayerModelToSlot{{box.interval, 0}, proc};
  cmd_layer->redo(context.context);
  context.dispatcher.submit(cmd_layer);

  const auto& devicelist = context.explorer.deviceModel().list();
  //// Setup listening on the curves ////
  for (const auto& vec : recordListening)
  {
    auto& dev = devicelist.device(*vec.front());
    if (!dev.connected())
      continue;

    std::vector<State::Address> addr_vec;
    addr_vec.reserve(vec.size());
    std::transform(vec.begin(), vec.end(), std::back_inserter(addr_vec), [](const auto& e) {
      return Device::address(*e).address;
    });
    dev.addToListening(addr_vec);

    // Add a custom callback.
    dev.valueUpdated.connect<&MessageRecorder::on_valueUpdated>(*this);

    m_recordCallbackConnections.push_back(&dev);
  }
  return true;
}
}
