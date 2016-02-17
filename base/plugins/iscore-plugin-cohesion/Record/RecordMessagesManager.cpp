#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Explorer/DocumentPlugin/ListeningState.hpp>

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

#include "Commands/Record.hpp"
#include "RecordMessagesManager.hpp"

#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/TreeNode.hpp>

#include <boost/optional/optional.hpp>
#include <core/document/Document.hpp>

#include <QApplication>
#include <qnamespace.h>
#include <QString>
#include <algorithm>
#include <type_traits>
#include <utility>

namespace Recording
{
RecordMessagesManager::RecordMessagesManager(const iscore::DocumentContext& ctx):
    m_ctx{ctx}
{
    m_recordTimer.setInterval(8);
    m_recordTimer.setTimerType(Qt::PreciseTimer);
}

void RecordMessagesManager::stopRecording()
{
    // Stop all the recording machinery
    m_recordTimer.stop();
    for(const auto& con : m_recordCallbackConnections)
    {
        QObject::disconnect(con);
    }

    qApp->processEvents();

    // Commit
    m_dispatcher->commit();

    m_explorer->deviceModel().resumeListening(m_savedListening);
}

void RecordMessagesManager::recordInNewBox(
        Scenario::ScenarioModel& scenar,
        Scenario::Point pt)
{
    auto& doc = iscore::IDocument::documentContext(scenar);
    //// Device tree management ////

    // Get all the selected nodes
    m_explorer = &Explorer::deviceExplorerFromContext(doc);
    auto indices = m_explorer->selectedIndexes(); // TODO maybe filterUniqueParents and then recurse on the listening ??

    // Disable listening for everything
    m_savedListening = m_explorer->deviceModel().pauseListening();

    // First get the addresses to listen.
    std::vector<std::vector<Device::FullAddressSettings>> recordListening;
    for(auto& index : indices)
    {
        // TODO use address settings instead.
        auto& node = m_explorer->nodeFromModelIndex(index);
        if(!node.is<Device::AddressSettings>())
            continue;

        auto addr = Device::address(node);
        // TODO shall we check if the address is in, out, recordable ?
        // Recording an automation of strings would actually have a meaning
        // here (for instance recording someone typing).

        // We sort the addresses by device to optimize.
        auto dev_it = std::find_if(recordListening.begin(),
                                   recordListening.end(),
                                   [&] (const auto& vec)
        { return vec.front().address.device == addr.device; });

        auto& as = node.get<Device::AddressSettings>();
        if(dev_it != recordListening.end())
        {
            dev_it->push_back(Device::FullAddressSettings::make<Device::FullAddressSettings::as_child>(as, addr));
        }
        else
        {
            recordListening.push_back({Device::FullAddressSettings::make<Device::FullAddressSettings::as_child>(as, addr)});
        }
    }

    if(recordListening.empty())
        return;

    m_dispatcher = std::make_unique<RecordCommandDispatcher>(
                new Recording::Record,
                doc.commandStack);

    //// Initial commands ////
    Box box = CreateBox(scenar, pt, *m_dispatcher);

    //// Creation of the process ////
    // Note : since we directly create the IDs here, we don't have to worry
    // about their generation.
    auto cmd_proc = new Scenario::Command::AddOnlyProcessToConstraint{
            Path<Scenario::ConstraintModel>(box.constraint),
            Metadata<ConcreteFactoryKey_k, RecordedMessages::ProcessModel>::get()};

    cmd_proc->redo();
    m_dispatcher->submitCommand(cmd_proc);

    auto& proc = box.constraint.processes.at(cmd_proc->processId());
    auto& record_proc = static_cast<RecordedMessages::ProcessModel&>(proc);
    m_createdProcess = &record_proc;

    //// Creation of the layer ////
    auto cmd_layer = new Scenario::Command::AddLayerModelToSlot{box.slot, proc};
    cmd_layer->redo();
    m_dispatcher->submitCommand(cmd_layer);

    const auto& devicelist = m_explorer->deviceModel().list();
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
        dev.replaceListening(addr_vec);

        // Add a custom callback.
        m_recordCallbackConnections.push_back(
                    connect(&dev, &Device::DeviceInterface::valueUpdated,
                this, [=] (const State::Address& addr, const State::Value& val) {

            if(!m_firstValueReceived)
            {
                m_firstValueReceived = true;
                start_time_pt = std::chrono::steady_clock::now();
                m_recordTimer.start();

                m_records.append(RecordedMessages::RecordedMessage{
                                     TimeValue::fromMsecs(0),
                                     State::Message{addr, val}});

            }
            else
            {
                auto current_time_pt = std::chrono::steady_clock::now();

                // Move end event by the current duration.
                double msecs = std::chrono::duration_cast<std::chrono::nanoseconds>(current_time_pt - start_time_pt).count() / 1000.;

                m_records.append(RecordedMessages::RecordedMessage{
                                     TimeValue::fromMsecs(msecs),
                                     State::Message{addr, val}});

                static_cast<RecordedMessages::ProcessModel*>(m_createdProcess)->setDuration(TimeValue::fromMsecs(msecs));
            }
        }));
    }

    //// Start the record timer ////
    connect(&m_recordTimer, &QTimer::timeout,
            this, [=] () {
        auto current_time_pt = std::chrono::steady_clock::now();

        // Move end event by the current duration.
        int msecs = std::chrono::duration_cast<std::chrono::milliseconds>(current_time_pt - start_time_pt).count();
        box.moveCommand.update(
                    Path<Scenario::ScenarioModel>{},
                    Id<Scenario::ConstraintModel>{},
                    box.endEvent,
                    pt.date + TimeValue::fromMsecs(msecs),
                    0,
                    true);

        box.moveCommand.redo();
    });

    // In case where the software is exited
    // during recording.
    connect(&scenar, &IdentifiedObjectAbstract::identified_object_destroyed,
            this, [&] () {
        m_recordTimer.stop();
    });
}
}
