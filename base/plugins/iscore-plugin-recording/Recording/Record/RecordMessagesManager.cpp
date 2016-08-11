
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

#include <Recording/Commands/Record.hpp>
#include "RecordMessagesManager.hpp"
#include <Recording/RecordedMessages/Commands/EditMessages.hpp>
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/TreeNode.hpp>
#include <Explorer/Explorer/ListeningManager.hpp>
#include <iscore/tools/std/Optional.hpp>
#include <core/document/Document.hpp>

#include <QApplication>
#include <qnamespace.h>
#include <QString>
#include <algorithm>
#include <type_traits>
#include <utility>

namespace Recording
{
RecordMessagesManager::RecordMessagesManager(
        RecordContext& ctx):
    m_context{ctx}
{
    m_recordTimer.setInterval(32);
    m_recordTimer.setTimerType(Qt::PreciseTimer);
}

void RecordMessagesManager::stop()
{
    // Stop all the recording machinery
    m_recordTimer.stop();
    auto msecs = GetTimeDifferenceInDouble(start_time_pt);
    for(const auto& con : m_recordCallbackConnections)
    {
        QObject::disconnect(con);
    }
    m_recordCallbackConnections.clear();

    qApp->processEvents();

    // Record and then stop
    if(!m_firstValueReceived)
    {
        m_context.dispatcher.rollback();
        return;
    }


    for(auto& val : m_records)
    {
        val.percentage = val.percentage / msecs;
    }

    auto cmd = new RecordedMessages::EditMessages{
            *m_createdProcess,
            m_records};

    // Commit
    cmd->redo();
    m_context.dispatcher.submitCommand(cmd);
    m_context.dispatcher.commit();

    m_context.explorer.deviceModel().listening().restore();
}

void RecordMessagesManager::setup()
{
    using namespace std::chrono;
    //// Device tree management ////

    // Get the listening of the selected addresses
    auto recordListening = GetAddressesToRecordRecursive(m_context.explorer);
    if(recordListening.empty())
        return;

    // Disable listening for everything
    m_context.explorer.deviceModel().listening().stop();

    //// Initial commands ////
    Box box = CreateBox(m_context);

    //// Creation of the process ////
    // Note : since we directly create the IDs here, we don't have to worry
    // about their generation.
    auto cmd_proc = new Scenario::Command::AddOnlyProcessToConstraint{
            Path<Scenario::ConstraintModel>(box.constraint),
            Metadata<ConcreteFactoryKey_k, RecordedMessages::ProcessModel>::get()};

    cmd_proc->redo();
    m_context.dispatcher.submitCommand(cmd_proc);

    auto& proc = box.constraint.processes.at(cmd_proc->processId());
    auto& record_proc = static_cast<RecordedMessages::ProcessModel&>(proc);
    m_createdProcess = &record_proc;

    //// Creation of the layer ////
    auto cmd_layer = new Scenario::Command::AddLayerModelToSlot{
                     box.slot,
                     proc};
    cmd_layer->redo();
    m_context.dispatcher.submitCommand(cmd_layer);

    const auto& devicelist = m_context.explorer.deviceModel().list();
    //// Setup listening on the curves ////
    for(const auto& vec : recordListening)
    {
        auto& dev = devicelist.device(vec.front().address.device);
        if(!dev.connected())
            continue;

        std::vector<State::Address> addr_vec;
        addr_vec.reserve(vec.size());
        std::transform(vec.begin(), vec.end(),
                       std::back_inserter(addr_vec),
                       [] (const auto& e ) { return e.address; });
        dev.addToListening(addr_vec);

        // Add a custom callback.
        m_recordCallbackConnections.push_back(
                    connect(&dev, &Device::DeviceInterface::valueUpdated,
                this, [=] (const State::Address& addr, const State::Value& val) {
            if(!m_firstValueReceived)
            {
                emit requestPlay();
                m_firstValueReceived = true;
                start_time_pt = steady_clock::now();
                m_recordTimer.start();

                m_records.append(RecordedMessages::RecordedMessage{
                                     0.,
                                     State::Message{addr, val}});
            }
            else
            {
                // Move end event by the current duration.
                auto msecs = GetTimeDifferenceInDouble(start_time_pt);

                m_records.append(RecordedMessages::RecordedMessage{
                                     msecs,
                                     State::Message{addr, val}});

                m_createdProcess->setDuration(TimeValue::fromMsecs(msecs));
            }
        }));
    }

    //// Start the record timer ////
    connect(&m_recordTimer, &QTimer::timeout,
            this, [=] () {
        // Move end event by the current duration.
        box.moveCommand.update(
                    Path<Scenario::ProcessModel>{},
                    Id<Scenario::ConstraintModel>{},
                    box.endEvent,
                    m_context.point.date + GetTimeDifference(start_time_pt),
                    0,
                    true);

        box.moveCommand.redo();
    });

    // In case where the software is exited
    // during recording.
    connect(&m_context.scenario, &IdentifiedObjectAbstract::identified_object_destroyed,
            this, [&] () {
        m_recordTimer.stop();
    });
}
}
