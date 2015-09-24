#include "RecordManager.hpp"
#include "Singletons/DeviceExplorerInterface.hpp"

#include "Commands/Scenario/Creations/CreateTimeNode_Event_State.hpp"
#include "Commands/Scenario/Creations/CreateConstraint_State_Event_TimeNode.hpp"
#include "Commands/Constraint/AddProcessToConstraint.hpp"
#include "Commands/Scenario/Displacement/MoveNewEvent.hpp"
#include "Commands/Scenario/Displacement/MoveEvent.hpp"
#include "Commands/ChangeAddress.hpp"
#include "Commands/InitAutomation.hpp"

#include "Automation/AutomationModel.hpp"
#include "Curve/CurveModel.hpp"

#include "Curve/Commands/UpdateCurve.hpp"
#include "Curve/Segment/Linear/LinearCurveSegmentModel.hpp"
#include "Curve/Segment/PointArray/PointArrayCurveSegmentModel.hpp"

#include "Plugin/Panel/DeviceExplorerModel.hpp"

#include <iscore/document/DocumentInterface.hpp>
#include <core/document/Document.hpp>


RecordManager::RecordManager()
{
    m_recordTimer.setInterval(8);
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
            double newval = node.get<iscore::AddressSettings>().value.val.toDouble();

            const auto& proc_data = records.at(addr);
            proc_data.segment.addPoint(msecs, newval);

            segt.addPoint(0, newval);
            static_cast<AutomationModel*>(proc_data.curveModel.parent())->setDuration(TimeValue::fromMsecs(msecs));
        }

        // Conversion of the piecewise to segments, and
        // serialization.
        auto data = recorded.second.segment.piecewise();

        QVector<QByteArray> newSegments;
        newSegments.resize(data.size());
        int i = 0;
        for(const auto& segment : data)
        {
            Serializer<DataStream> s(&newSegments[i++]);
            s.readFrom(*static_cast<CurveSegmentModel*>(segment.get()));
        }

        // Add a point with the last state.
        auto initCurveCmd = new InitAutomation{
                iscore::IDocument::path(*safe_cast<AutomationModel*>(recorded.second.curveModel.parent())),
                recorded.first,
                recorded.second.segment.min(),
                recorded.second.segment.max(),
                newSegments};

        // This one shall not be redone
        m_dispatcher->submitCommand(recorded.second.addProcCmd);

        m_dispatcher->submitCommand(initCurveCmd);
        initCurveCmd->redo();
    }

    // Commit
    m_dispatcher->commit();

    m_explorer->deviceModel().resumeListening(m_savedListening);
}

void RecordManager::recordInNewBox(ScenarioModel& scenar, ScenarioPoint pt)
{
    auto& doc = *iscore::IDocument::documentFromObject(scenar);
    //// Device tree management ////

    // Get all the selected nodes
    m_explorer = doc.findChild<DeviceExplorerModel*>("DeviceExplorerModel");
    auto indices = m_explorer->selectedIndexes();

    // Disable listening for everything
    m_savedListening = m_explorer->deviceModel().pauseListening();

    // First get the addresses to listen.
    std::vector<std::vector<iscore::Address>> m_recordListening;
    for(auto& index : indices)
    {
        // TODO use address settings instead.
        auto addr = DeviceExplorer::addressFromModelIndex(index);
        // TODO shall we check if the address is in, out, recordable ?
        // Recording an automation of strings would actually have a meaning
        // here (for instance recording someone typing).

        // We sort the addresses by device to optimize.
        auto dev_it = std::find_if(m_recordListening.begin(),
                                   m_recordListening.end(),
                                   [&] (const auto& vec)
        { return vec.front().device == addr.device; });

        if(dev_it != m_recordListening.end())
        {
            dev_it->push_back(addr);
        }
        else
        {
            m_recordListening.push_back({addr});
        }
    }

    if(m_recordListening.empty())
        return;

    m_dispatcher = std::make_unique<QuietMacroCommandDispatcher>(new Record, doc.commandStack());

    //// Initial commands ////

    // Get the clicked point in scenario and create a state + constraint + state there
    // Create an automation + a rack + a slot + process views for all automations.
    auto default_end_date = pt.date;
    auto cmd_start = new CreateTimeNode_Event_State{
            scenar,
            pt.date,
            pt.y};
    cmd_start->redo();
    m_dispatcher->submitCommand(cmd_start);

    // TODO what happens if we go past the end of our scenario ? Stop recording ??
    auto cmd_end = new CreateConstraint_State_Event_TimeNode{
            scenar,
            cmd_start->createdState(),
            default_end_date,
            pt.y};
    cmd_end->redo();
    m_dispatcher->submitCommand(cmd_end);

    auto& cstr = scenar.constraints.at(cmd_end->createdConstraint());
    auto cstr_path = iscore::IDocument::path(cstr);

    auto cmd_move = new Scenario::Command::MoveNewEvent(
                iscore::IDocument::path(scenar),
                cstr.id(),
                cmd_end->createdEvent(),
                default_end_date,
                0,
                true);
    m_dispatcher->submitCommand(cmd_move);

    //// Creation of the curves ////
    for(const auto& vec : m_recordListening)
    {
        for(const auto& addr : vec)
        {
            auto cmd_proc = new AddProcessToConstraint{
                    Path<ConstraintModel>(cstr_path),
                    "Automation"};
            cmd_proc->redo();

            auto& proc = cstr.processes.at(cmd_proc->processId());
            auto& autom = static_cast<AutomationModel&>(proc);
            autom.curve().clear();
            auto segt = new PointArrayCurveSegmentModel{
                    Id<CurveSegmentModel>{0},
                    &autom.curve()};

            segt->setStart({0, -1});
            segt->setEnd({1, -1});

            autom.curve().addSegment(segt);

            segt->addPoint(0, getNodeFromAddress(m_explorer->rootNode(), addr).get<iscore::AddressSettings>().value.val.toDouble());

            // TODO fetch initial min / max from AddressSettings ?
            records.insert(
                        std::make_pair(
                            addr,
                            RecordData{
                                cmd_proc,
                                autom.curve(),
                                *segt}));
        }
    }

    //// Setup listening on the curves ////
    for(const auto& vec : m_recordListening)
    {
        auto& dev = m_explorer->deviceModel().list().device(vec.front().device);

        dev.replaceListening(vec);
        // Add a custom callback.
        m_recordCallbackConnections.push_back(
                    connect(&dev, &DeviceInterface::valueUpdated,
                this, [=] (const iscore::Address& addr, const iscore::Value& val) {
            auto current_time_pt = std::chrono::steady_clock::now();

            // Move end event by the current duration.
            int msecs = std::chrono::duration_cast<std::chrono::milliseconds>(current_time_pt - start_time_pt).count();

            bool ok = false;
            double newval = val.val.toDouble(&ok);
            if(!ok)
                return;

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
                    Path<ScenarioModel>{},
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
