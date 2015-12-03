#include <Automation/AutomationModel.hpp>
#include <Automation/Commands/InitAutomation.hpp>
#include <Curve/Segment/PointArray/PointArrayCurveSegmentModel.hpp>
#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Explorer/DeviceExplorerModel.hpp>
#include <Scenario/Commands/Constraint/AddOnlyProcessToConstraint.hpp>
#include <Scenario/Commands/Constraint/AddRackToConstraint.hpp>
#include <Scenario/Commands/Constraint/Rack/AddSlotToRack.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateConstraint_State_Event_TimeNode.hpp>
#include <Scenario/Commands/Scenario/Creations/CreateTimeNode_Event_State.hpp>
#include <Scenario/Commands/Scenario/Displacement/MoveNewEvent.hpp>
#include <Scenario/Commands/Scenario/ShowRackInViewModel.hpp>
#include <boost/optional/optional.hpp>
#include <core/document/Document.hpp>

#include <QApplication>
#include <qnamespace.h>
#include <QString>
#include <algorithm>
#include <type_traits>
#include <utility>

#include <Automation/AutomationProcessMetadata.hpp>
#include "Commands/Record.hpp"
#include <Curve/CurveModel.hpp>
#include <Device/Address/AddressSettings.hpp>
#include <Device/Address/IOType.hpp>
#include <Device/Node/DeviceNode.hpp>
#include <Device/Protocol/DeviceInterface.hpp>
#include <Device/Protocol/DeviceList.hpp>
#include <Explorer/DocumentPlugin/ListeningState.hpp>
#include <Process/ExpandMode.hpp>
#include <Process/Process.hpp>
#include <Process/TimeValue.hpp>
#include "Record/RecordData.hpp"
#include "RecordManager.hpp"
#include <Scenario/Commands/Constraint/Rack/Slot/AddLayerModelToSlot.hpp>
#include <Scenario/Document/Constraint/ConstraintModel.hpp>
#include <Scenario/Document/Constraint/Rack/RackModel.hpp>
#include <Scenario/Document/Constraint/Rack/Slot/SlotModel.hpp>
#include <Scenario/Palette/ScenarioPoint.hpp>
#include <Scenario/Process/ScenarioModel.hpp>
#include <State/Value.hpp>
#include <State/ValueConversion.hpp>
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <iscore/document/DocumentInterface.hpp>
#include <iscore/tools/ModelPath.hpp>
#include <iscore/tools/NotifyingMap.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/tools/TreeNode.hpp>

class CurveSegmentModel;

RecordManager::RecordManager()
{
    m_recordTimer.setInterval(8);
    m_recordTimer.setTimerType(Qt::PreciseTimer);
}

void RecordManager::stopRecording()
{
    // Stop all the recording machinery
    m_recordTimer.stop();
    for(const auto& con : m_recordCallbackConnections)
    {
        QObject::disconnect(con);
    }

    qApp->processEvents();

    // Add a last point corresponding to the current state

    // Create commands for the state of each automation to send on
    // the network, and push them silently.

    // Potentially simplify curve and transform it in segments
    for(const auto& recorded : records)
    {
        const auto& addr = recorded.first;
        auto& segt = recorded.second.segment;

        // Here we add a last point with the current state of the things.
        {
            auto current_time_pt = std::chrono::steady_clock::now();

            // Move end event by the current duration.
            int msecs = std::chrono::duration_cast<std::chrono::milliseconds>(current_time_pt - start_time_pt).count();

            const auto& node = getNodeFromAddress(m_explorer->rootNode(), addr);
            double newval = iscore::convert::value<double>(node.get<iscore::AddressSettings>().value);

            const auto& proc_data = records.at(addr);
            segt.addPoint(msecs, newval);

            static_cast<AutomationModel*>(proc_data.curveModel.parent())->setDuration(TimeValue::fromMsecs(msecs));
        }

        // Conversion of the piecewise to segments, and
        // serialization.
        recorded.second.segment.simplify();
        // TODO if there is no remaining segment or an invalid segment, don't add it.

        // Add a point with the last state.
        auto initCurveCmd = new InitAutomation{
                *safe_cast<AutomationModel*>(recorded.second.curveModel.parent()),
                recorded.first,
                recorded.second.segment.min(),
                recorded.second.segment.max(),
                recorded.second.segment.toLinearSegments()};

        // This one shall not be redone
        m_dispatcher->submitCommand(recorded.second.addProcCmd);
        m_dispatcher->submitCommand(recorded.second.addLayCmd);
        m_dispatcher->submitCommand(initCurveCmd);
    }

    // Commit
    m_dispatcher->commit();

    m_explorer->deviceModel().resumeListening(m_savedListening);
}

void RecordManager::recordInNewBox(Scenario::ScenarioModel& scenar, Scenario::Point pt)
{
    auto& doc = iscore::IDocument::documentContext(scenar);
    //// Device tree management ////

    // Get all the selected nodes
    m_explorer = &deviceExplorerFromContext(doc);
    auto indices = m_explorer->selectedIndexes(); // TODO maybe filterUniqueParents and then recurse on the listening ??

    // Disable listening for everything
    m_savedListening = m_explorer->deviceModel().pauseListening();

    // First get the addresses to listen.
    std::vector<std::vector<iscore::Address>> recordListening;
    for(auto& index : indices)
    {
        // TODO use address settings instead.
        auto& node = m_explorer->nodeFromModelIndex(index);
        auto addr = iscore::address(node);
        // TODO shall we check if the address is in, out, recordable ?
        // Recording an automation of strings would actually have a meaning
        // here (for instance recording someone typing).

        // We sort the addresses by device to optimize.
        auto dev_it = std::find_if(recordListening.begin(),
                                   recordListening.end(),
                                   [&] (const auto& vec)
        { return vec.front().device == addr.device; });


        if(node.get<iscore::AddressSettings>().value.val.isNumeric()
        && hasInput(node.get<iscore::AddressSettings>().ioType))
        {
            if(dev_it != recordListening.end())
            {
                dev_it->push_back(addr);
            }
            else
            {
                recordListening.push_back({addr});
            }
        }
    }

    if(recordListening.empty())
        return;

    m_dispatcher = std::make_unique<RecordCommandDispatcher>(new Record, doc.commandStack);

    //// Initial commands ////

    // Get the clicked point in scenario and create a state + constraint + state there
    // Create an automation + a rack + a slot + process views for all automations.
    auto default_end_date = pt.date;
    auto cmd_start = new Scenario::Command::CreateTimeNode_Event_State{
            scenar,
            pt.date,
            pt.y};
    cmd_start->redo();
    m_dispatcher->submitCommand(cmd_start);

    // TODO what happens if we go past the end of our scenario ? Stop recording ??
    auto cmd_end = new Scenario::Command::CreateConstraint_State_Event_TimeNode{
            scenar,
            cmd_start->createdState(),
            default_end_date,
            pt.y};
    cmd_end->redo();
    m_dispatcher->submitCommand(cmd_end);

    auto& cstr = scenar.constraints.at(cmd_end->createdConstraint());
    Path<ConstraintModel> cstr_path{cstr};

    auto cmd_move = new Scenario::Command::MoveNewEvent(
                scenar,
                cstr.id(),
                cmd_end->createdEvent(),
                default_end_date,
                0,
                true,
                ExpandMode::Fixed);
    m_dispatcher->submitCommand(cmd_move);

    auto cmd_rack = new Scenario::Command::AddRackToConstraint{cstr};
    cmd_rack->redo();
    m_dispatcher->submitCommand(cmd_rack);
    auto& rack = cstr.racks.at(cmd_rack->createdRack());
    auto cmd_slot = new Scenario::Command::AddSlotToRack{rack};
    cmd_slot->redo();
    m_dispatcher->submitCommand(cmd_slot);

    for(const auto& vm : cstr.viewModels())
    {
        auto cmd_showrack = new Scenario::Command::ShowRackInViewModel{*vm, rack.id()};
        cmd_showrack->redo();
        m_dispatcher->submitCommand(cmd_showrack);
    }


    auto& slot = rack.slotmodels.at(cmd_slot->createdSlot());
    //// Creation of the curves ////
    for(const auto& vec : recordListening)
    {
        for(const auto& addr : vec)
        {
            // Note : since we directly create the IDs here, we don't have to worry
            // about their generation.
            auto cmd_proc = new AddOnlyProcessToConstraint{
                    Path<ConstraintModel>(cstr_path),
                    AutomationProcessMetadata::factoryKey()};
            cmd_proc->redo();
            auto& proc = cstr.processes.at(cmd_proc->processId());
            auto& autom = static_cast<AutomationModel&>(proc);


            auto cmd_layer = new Scenario::Command::AddLayerModelToSlot{slot, proc};
            cmd_layer->redo();

            autom.curve().clear();
            auto segt = new PointArrayCurveSegmentModel{
                    Id<CurveSegmentModel>{0},
                    &autom.curve()};

            segt->setStart({0, -1});
            segt->setEnd({1, -1});

            autom.curve().addSegment(segt);

            segt->addPoint(0, iscore::convert::value<float>(getNodeFromAddress(m_explorer->rootNode(), addr).get<iscore::AddressSettings>().value));

            // TODO fetch initial min / max from AddressSettings ?
            records.insert(
                        std::make_pair(
                            addr,
                            RecordData{
                                cmd_proc,
                                cmd_layer,
                                autom.curve(),
                                *segt}));
        }
    }

    const auto& devicelist = m_explorer->deviceModel().list();
    //// Setup listening on the curves ////
    for(const auto& vec : recordListening)
    {
        auto& dev = devicelist.device(vec.front().device);
        if(!dev.connected())
            continue;

        dev.replaceListening(vec);
        // Add a custom callback.
        m_recordCallbackConnections.push_back(
                    connect(&dev, &DeviceInterface::valueUpdated,
                this, [=] (const iscore::Address& addr, const iscore::Value& val) {
            auto current_time_pt = std::chrono::steady_clock::now();

            // Move end event by the current duration.
            int msecs = std::chrono::duration_cast<std::chrono::milliseconds>(current_time_pt - start_time_pt).count();

            auto newval = iscore::convert::value<float>(val.val);

            const auto& proc_data = records.at(addr);
            proc_data.segment.addPoint(msecs, newval);

            static_cast<AutomationModel*>(proc_data.curveModel.parent())->setDuration(TimeValue::fromMsecs(msecs));
        }));
    }

    //// Start the record timer ////
    connect(&m_recordTimer, &QTimer::timeout,
            this, [=] () {
        auto current_time_pt = std::chrono::steady_clock::now();

        // Move end event by the current duration.
        int msecs = std::chrono::duration_cast<std::chrono::milliseconds>(current_time_pt - start_time_pt).count();
        cmd_move->update(
                    Path<Scenario::ScenarioModel>{},
                    Id<ConstraintModel>{},
                    cmd_end->createdEvent(),
                    pt.date + TimeValue::fromMsecs(msecs),
                    0,
                    true);

        cmd_move->redo();
    });

    start_time_pt = std::chrono::steady_clock::now();
    m_recordTimer.start();
}
