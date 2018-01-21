// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <Scenario/Commands/Interval/AddOnlyProcessToInterval.hpp>
#include <Scenario/Commands/Interval/Rack/Slot/AddLayerModelToSlot.hpp>

#include <Scenario/Document/Interval/IntervalModel.hpp>
#include <Scenario/Document/Interval/Slot.hpp>

#include <Scenario/Commands/Scenario/Displacement/MoveNewEvent.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveNewState.hpp>

#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Process/ScenarioModel.hpp>

#include <Device/Address/AddressSettings.hpp>
#include <Device/Address/IOType.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceList.hpp>

#include <Process/ExpandMode.hpp>
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>

#include <State/Value.hpp>
#include <State/ValueConversion.hpp>

#include "RecordMessagesManager.hpp"
#include <Explorer/Explorer/ListeningManager.hpp>
#include <Recording/Commands/Record.hpp>
#include <core/document/Document.hpp>
#include <score/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <score/model/EntityMap.hpp>
#include <score/model/path/Path.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/tree/TreeNode.hpp>
#include <score/tools/std/Optional.hpp>

#include <QApplication>
#include <QString>
#include <algorithm>
#include <qnamespace.h>
#include <type_traits>
#include <utility>
#include <Scenario/Commands/Scenario/Creations/CreateInterval_State_Event_TimeSync.hpp>
#include <Scenario/Commands/State/AddMessagesToState.hpp>
namespace Recording
{
MessageRecorder::MessageRecorder(RecordContext& ctx) : context{ctx}
{
}

void MessageRecorder::stop()
{
  // Stop all the recording machinery
  for (const auto& dev : m_recordCallbackConnections)
  {
    if (dev)
      (*dev)
          .valueUpdated
          .disconnect<MessageRecorder, &MessageRecorder::on_valueUpdated>(
              *this);
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
  auto setStateCmd = new Scenario::Command::AddMessagesToState(m_createdProcess->state(startState), {m_records[0].m});
  setStateCmd->redo(context.context);
  context.dispatcher.submitCommand(setStateCmd);

  auto movecmd = new Scenario::Command::MoveNewState{*m_createdProcess, startState, 0.5};
  movecmd->redo(context.context);
  context.dispatcher.submitCommand(movecmd);

  for (std::size_t i = 1; i < m_records.size(); i++)
  {
    RecordedMessage& val = m_records[i];

    // Create a state
    auto cmd = new Scenario::Command::CreateInterval_State_Event_TimeSync{
               *m_createdProcess,
               startState,
               TimeVal::fromMsecs(val.percentage),
               0.5};
    cmd->redo(context.context);
    startState = cmd->createdState();
    context.dispatcher.submitCommand(cmd);

    // Add messages to it
    auto setStateCmd = new Scenario::Command::AddMessagesToState(m_createdProcess->state(startState), {val.m});
    setStateCmd->redo(context.context);
    context.dispatcher.submitCommand(setStateCmd);

  }

}

void MessageRecorder::on_valueUpdated(
    const State::Address& addr, const ossia::value& val)
{
  if (context.started())
  {
    // Move end event by the current duration.
    auto msecs = context.timeInDouble();

    m_records.push_back(RecordedMessage{
        msecs, State::Message{State::AddressAccessor{addr},
                              val}});

    m_createdProcess->setDuration(TimeVal::fromMsecs(msecs));
  }
  else
  {
    emit firstMessageReceived();
    context.start();

    m_records.push_back(RecordedMessage{
        0., State::Message{State::AddressAccessor{addr},
                           val}});
  }
}

bool MessageRecorder::setup(
    const Box& box, const RecordListening& recordListening)
{
  using namespace std::chrono;
  //// Device tree management ////

  //// Creation of the process ////
  // Note : since we directly create the IDs here, we don't have to worry
  // about their generation.
  auto cmd_proc = new Scenario::Command::AddOnlyProcessToInterval{
      box.interval,
                  Metadata<ConcreteKey_k, Scenario::ProcessModel>::get(), {}};

  cmd_proc->redo(context.context);
  context.dispatcher.submitCommand(cmd_proc);

  auto& proc = box.interval.processes.at(cmd_proc->processId());
  auto& record_proc = static_cast<Scenario::ProcessModel&>(proc);
  m_createdProcess = &record_proc;

  //// Creation of the layer ////
  auto cmd_layer = new Scenario::Command::AddLayerModelToSlot{{box.interval, 0}, proc};
  cmd_layer->redo(context.context);
  context.dispatcher.submitCommand(cmd_layer);

  const auto& devicelist = context.explorer.deviceModel().list();
  //// Setup listening on the curves ////
  for (const auto& vec : recordListening)
  {
    auto& dev = devicelist.device(*vec.front());
    if (!dev.connected())
      continue;

    std::vector<State::Address> addr_vec;
    addr_vec.reserve(vec.size());
    std::transform(
        vec.begin(), vec.end(), std::back_inserter(addr_vec),
        [](const auto& e) { return Device::address(*e).address; });
    dev.addToListening(addr_vec);

    // Add a custom callback.
    dev.valueUpdated
        .connect<MessageRecorder, &MessageRecorder::on_valueUpdated>(*this);

    m_recordCallbackConnections.push_back(&dev);
  }
  return true;
}
}
