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
#include <iscore/tools/std/Optional.hpp>
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

#include <Explorer/Explorer/ListeningManager.hpp>
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
#include <Curve/Settings/Model.hpp>
namespace Curve
{
class SegmentModel;
}

namespace Recording
{
RecordManager::RecordManager(
        const iscore::DocumentContext& ctx):
    m_ctx{ctx},
    m_settings{m_ctx.app.settings<Curve::Settings::Model>()}
{
    m_recordTimer.setInterval(8);
    m_recordTimer.setTimerType(Qt::PreciseTimer);
}

void RecordManager::stopRecording()
{
    // Stop all the recording machinery
    m_recordTimer.stop();
    auto msecs = GetTimeDifference(start_time_pt);
    for(const auto& con : m_recordCallbackConnections)
    {
        QObject::disconnect(con);
    }
    m_recordCallbackConnections.clear();

    qApp->processEvents();

    // Nothing was being recorded
    if(!m_dispatcher)
        return;

    // Record and then stop
    if(!m_firstValueReceived)
    {
        m_dispatcher->rollback();
        return;
    }

    auto simplify = m_settings.getSimplify();
    auto simplifyRatio = m_settings.getSimplificationRatio();
    // Add a last point corresponding to the current state

    // Create commands for the state of each automation to send on
    // the network, and push them silently.

    // Potentially simplify curve and transform it in segments
    for(const auto& recorded : records)
    {
        const auto& addr = recorded.first;
        auto& segt = recorded.second.segment;

        auto& automation = *safe_cast<Automation::ProcessModel*>(
                    recorded.second.curveModel.parent());

        // Here we add a last point with the current state of the things.
        {
            // Move end event by the current duration.
            const auto& node = getNodeFromAddress(m_explorer->rootNode(), addr.address);
            double newval = State::convert::value<double>(node.get<Device::AddressSettings>().value);

            // Maybe add first point
            if(!segt.points().empty())
            {
                auto begin_pt = *segt.points().begin();
                if(begin_pt.first != 0.)
                    segt.addPoint(0., begin_pt.second);
            }
            else
            {
                segt.addPoint(0, newval);
            }
            // Add last point
            segt.addPoint(msecs.msec(), newval);

            automation.setDuration(msecs);
        }

        // Conversion of the piecewise to segments, and
        // serialization.
        if(simplify)
            recorded.second.segment.simplify(simplifyRatio);
        // TODO if there is no remaining segment or an invalid segment, don't add it.

        // Add a point with the last state.
        auto initCurveCmd = new Automation::InitAutomation{
                automation,
                recorded.first.address,
                recorded.second.segment.min(),
                recorded.second.segment.max(),
                recorded.second.segment.toPowerSegments()};

        // This one shall not be redone
        m_dispatcher->submitCommand(recorded.second.addProcCmd);
        m_dispatcher->submitCommand(recorded.second.addLayCmd);
        m_dispatcher->submitCommand(initCurveCmd);
    }

    // Commit
    m_dispatcher->commit();

    m_explorer->deviceModel().listening().restore();
}

void RecordManager::messageCallback(const State::Address &addr, const State::Value &val)
{
    using namespace std::chrono;
    if(m_firstValueReceived)
    {
        auto msecs = GetTimeDifference(start_time_pt);

        auto newval = State::convert::value<float>(val.val);

        auto it = find_if(records, [&] (const auto& elt) { return elt.first.address == addr; });
        ISCORE_ASSERT(it != records.end());

        const auto& proc_data = it->second;

        proc_data.segment.addPoint(msecs.msec(), newval);

        static_cast<Automation::ProcessModel*>(proc_data.curveModel.parent())->setDuration(msecs);
    }
    else
    {
        m_firstValueReceived = true;
        start_time_pt = steady_clock::now();
        m_recordTimer.start();

        auto newval = State::convert::value<float>(val.val);

        auto it = find_if(records, [&] (const auto& elt) { return elt.first.address == addr; });
        ISCORE_ASSERT(it != records.end());

        const auto& proc_data = it->second;
        proc_data.segment.addPoint(0, newval);
    }
}

void RecordManager::parameterCallback(const State::Address &addr, const State::Value &val)
{
    using namespace std::chrono;
    if(m_firstValueReceived)
    {
        auto msecs = GetTimeDifference(start_time_pt);

        auto newval = State::convert::value<float>(val.val);

        auto it = find_if(records, [&] (const auto& elt) { return elt.first.address == addr; });
        ISCORE_ASSERT(it != records.end());

        const auto& proc_data = it->second;

        auto last = proc_data.segment.points().rbegin();
        proc_data.segment.addPoint(msecs.msec() - 1, last->second);
        proc_data.segment.addPoint(msecs.msec(), newval);

        static_cast<Automation::ProcessModel*>(proc_data.curveModel.parent())->setDuration(msecs);
    }
    else
    {
        m_firstValueReceived = true;
        start_time_pt = steady_clock::now();
        m_recordTimer.start();

        auto newval = State::convert::value<float>(val.val);

        auto it = find_if(records, [&] (const auto& elt) { return elt.first.address == addr; });
        ISCORE_ASSERT(it != records.end());

        const auto& proc_data = it->second;
        proc_data.segment.addPoint(0, newval);
    }
}

void RecordManager::recordInNewBox(
        const Scenario::ScenarioModel& scenar,
        Scenario::Point pt)
{
    using namespace std::chrono;
    auto& doc = iscore::IDocument::documentContext(scenar);
    //// Device tree management ////

    // Get all the selected nodes
    m_explorer = &Explorer::deviceExplorerFromContext(doc);

    // Get the listening of the selected addresses
    auto recordListening = GetAddressesToRecord(*m_explorer);
    if(recordListening.empty())
        return;

    // Disable listening for everything
    m_explorer->deviceModel().listening().stop();

    m_dispatcher = std::make_unique<RecordCommandDispatcher>(new Recording::Record, doc.commandStack);

    //// Initial commands ////
    Box box = CreateBox(scenar, pt, *m_dispatcher);

    //// Creation of the curves ////
    for(const auto& vec : recordListening)
    {
        for(const auto& addr : vec)
        {
            // Note : since we directly create the IDs here, we don't have to worry
            // about their generation.
            auto cmd_proc = new Scenario::Command::AddOnlyProcessToConstraint{
                    Path<Scenario::ConstraintModel>(box.constraint),
                    Metadata<ConcreteFactoryKey_k, Automation::ProcessModel>::get()};
            cmd_proc->redo();
            auto& proc = box.constraint.processes.at(cmd_proc->processId());
            auto& autom = static_cast<Automation::ProcessModel&>(proc);


            auto cmd_layer = new Scenario::Command::AddLayerModelToSlot{box.slot, proc};
            cmd_layer->redo();

            autom.curve().clear();
            auto segt = new Curve::PointArraySegment{
                    Id<Curve::SegmentModel>{0},
                    &autom.curve()};

            auto val = State::convert::value<float>(addr.value);
            segt->setStart({0, val});
            segt->setEnd({1, -1});
            segt->addPoint(0, val);

            autom.curve().addSegment(segt);

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
    auto callback_to_use =
            m_settings.getMode() == Curve::Settings::Mode::Parameter
            ? &RecordManager::parameterCallback
            : &RecordManager::messageCallback;

    for(const auto& vec : recordListening)
    {
        auto& dev = devicelist.device(vec.front().address.device);
        if(!dev.connected())
            continue;

        std::vector<State::Address> addr_vec;
        addr_vec.reserve(vec.size());
        std::transform(vec.begin(), vec.end(),
                       std::back_inserter(addr_vec),
                       [] (const auto& e) { return e.address; });
        dev.addToListening(addr_vec);
        // Add a custom callback.
        m_recordCallbackConnections.push_back(
                    connect(&dev, &Device::DeviceInterface::valueUpdated,
                this, callback_to_use));
    }

    //// Start the record timer ////
    connect(&m_recordTimer, &QTimer::timeout,
            this, [=] () {
        // Move end event by the current duration.
        box.moveCommand.update(
                    {},
                    {},
                    box.endEvent,
                    pt.date + GetTimeDifference(start_time_pt),
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
