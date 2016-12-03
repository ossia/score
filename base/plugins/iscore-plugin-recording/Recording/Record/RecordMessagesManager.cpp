
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>

#include <Scenario/Commands/Constraint/AddOnlyProcessToConstraint.hpp>
#include <Scenario/Commands/Constraint/Rack/Slot/AddLayerModelToSlot.hpp>

#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>

#include <Scenario/Commands/Scenario/Displacement/MoveNewEvent.hpp>

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
#include <Recording/RecordedMessages/Commands/EditMessages.hpp>
#include <core/document/Document.hpp>
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/tools/EntityMap.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/TreeNode.hpp>
#include <iscore/tools/std/Optional.hpp>

#include <QApplication>
#include <QString>
#include <algorithm>
#include <qnamespace.h>
#include <type_traits>
#include <utility>

namespace Recording
{
MessageRecorder::MessageRecorder(RecordContext& ctx) : context{ctx}
{
}

void MessageRecorder::stop()
{
  // Stop all the recording machinery
  auto msecs = context.timeInDouble();
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

  for (auto& val : m_records)
  {
    val.percentage = val.percentage / msecs;
  }

  auto cmd = new RecordedMessages::EditMessages{*m_createdProcess, m_records};

  // Commit
  cmd->redo();
  context.dispatcher.submitCommand(cmd);
}

void MessageRecorder::on_valueUpdated(
    const State::Address& addr, const ossia::value& val)
{
  if (context.started())
  {
    // Move end event by the current duration.
    auto msecs = context.timeInDouble();

    m_records.append(RecordedMessages::RecordedMessage{
        msecs, State::Message{State::AddressAccessor{addr},
                              State::fromOSSIAValue(val)}});

    m_createdProcess->setDuration(TimeValue::fromMsecs(msecs));
  }
  else
  {
    emit firstMessageReceived();
    context.start();

    m_records.append(RecordedMessages::RecordedMessage{
        0., State::Message{State::AddressAccessor{addr},
                           State::fromOSSIAValue(val)}});
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
  auto cmd_proc = new Scenario::Command::AddOnlyProcessToConstraint{
      Path<Scenario::ConstraintModel>(box.constraint),
      Metadata<ConcreteFactoryKey_k, RecordedMessages::ProcessModel>::get()};

  cmd_proc->redo();
  context.dispatcher.submitCommand(cmd_proc);

  auto& proc = box.constraint.processes.at(cmd_proc->processId());
  auto& record_proc = static_cast<RecordedMessages::ProcessModel&>(proc);
  m_createdProcess = &record_proc;

  //// Creation of the layer ////
  auto cmd_layer = new Scenario::Command::AddLayerModelToSlot{box.slot, proc};
  cmd_layer->redo();
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
