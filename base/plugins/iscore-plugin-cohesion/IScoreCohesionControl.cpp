#include "IScoreCohesionControl.hpp"
#include <QApplication>

#include "base/plugins/iscore-plugin-scenario/source/Document/Constraint/ViewModels/ConstraintViewModel.hpp"
#include "base/plugins/iscore-plugin-scenario/source/Document/Constraint/ViewModels/ConstraintPresenter.hpp"
#include "base/plugins/iscore-plugin-scenario/source/Document/Constraint/ConstraintModel.hpp"
#include "base/plugins/iscore-plugin-scenario/source/Document/Event/EventModel.hpp"
#include "base/plugins/iscore-plugin-scenario/source/Document/BaseElement/BaseElementPresenter.hpp"
#include "base/plugins/iscore-plugin-scenario/source/Process/ScenarioModel.hpp"
#include "base/plugins/iscore-plugin-scenario/source/Commands/Event/AddStateToEvent.hpp"
#include "Singletons/DeviceExplorerInterface.hpp"
#include "Automation/AutomationModel.hpp"
// TODO Refactor in order to use the Node data structure instead.
#include <DeviceExplorer/../Plugin/Panel/DeviceExplorerModel.hpp>
#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>

#include "Commands/CreateStatesFromParametersInEvents.hpp"

#include <Commands/CreateCurvesFromAddresses.hpp>
#include <Commands/CreateCurvesFromAddressesInConstraints.hpp>
#include "Commands/CreateCurveFromStates.hpp"
#include <core/document/DocumentPresenter.hpp>
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <source/Document/BaseElement/BaseElementModel.hpp>
#include <QKeySequence>
#include <iscore/command/CommandGeneratorMap.hpp>
#include "Plugin/Commands/AddMessagesToModel.hpp"
#include "Control/ScenarioControl.hpp"

#include <core/document/DocumentModel.hpp>
#include <QToolBar>

using namespace iscore;
IScoreCohesionControl::IScoreCohesionControl(Presenter* pres) :
    iscore::PluginControlInterface {pres, "IScoreCohesionControl", nullptr}
{
    // Since we have declared the dependency, we can assume
    // that ScenarioControl is instantiated already.
    auto scen = ScenarioControl::instance();
    connect(scen, &ScenarioControl::startRecording,
            this, &IScoreCohesionControl::record);


    setupCommands();

    m_snapshot = new QAction {tr("Snapshot in Event"), this};
    m_snapshot->setShortcutContext(Qt::ApplicationShortcut);
    m_snapshot->setShortcut(tr("Ctrl+J"));
    m_snapshot->setToolTip(tr("Ctrl+J"));
    connect(m_snapshot, &QAction::triggered,
            this, &IScoreCohesionControl::snapshotParametersInStates);

    m_interp = new QAction {tr("Interpolate states"), this};
    m_interp->setShortcutContext(Qt::ApplicationShortcut);
    m_interp->setShortcut(tr("Ctrl+K"));
    m_interp->setToolTip(tr("Ctrl+K"));
    connect(m_interp, &QAction::triggered,
            this, &IScoreCohesionControl::interpolateStates);

    m_curves = new QAction {tr("Create Curves"), this};
    m_curves->setShortcutContext(Qt::ApplicationShortcut);
    m_curves->setShortcut(tr("Ctrl+L"));
    m_curves->setToolTip(tr("Ctrl+L"));
    connect(m_curves, &QAction::triggered,
            this, &IScoreCohesionControl::createCurvesFromAddresses);

}

void IScoreCohesionControl::populateMenus(iscore::MenubarManager* menu)
{
    // If there is the Curve plug-in, the Device Explorer, and the Scenario plug-in,
    // We can add an option in the menu to generate curves from the selected addresses
    // in the current constraint.

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::ObjectMenu,
                                       m_curves);

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::ObjectMenu,
                                       m_snapshot);

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::ObjectMenu,
                                       m_interp);
}

QList<OrderedToolbar> IScoreCohesionControl::makeToolbars()
{
    QToolBar* bar = new QToolBar;
    bar->addActions({m_curves, m_snapshot, m_interp});
    return QList<OrderedToolbar>{OrderedToolbar(2, bar)};
}


struct IScoreionCohesionCommandFactory
{
        static CommandGeneratorMap map;
};

CommandGeneratorMap IScoreionCohesionCommandFactory::map;

void IScoreCohesionControl::setupCommands()
{
    boost::mpl::for_each<
            boost::mpl::list<
                CreateCurvesFromAddresses,
                CreateCurvesFromAddressesInConstraints,
                InterpolateMacro,
                CreateStatesFromParametersInEvents
            >,
            boost::type<boost::mpl::_>
    >(CommandGeneratorMapInserter<IScoreionCohesionCommandFactory>());
}

SerializableCommand* IScoreCohesionControl::instantiateUndoCommand(const QString& name, const QByteArray& data)
{
    return PluginControlInterface::instantiateUndoCommand<IScoreionCohesionCommandFactory>(name, data);
}

void IScoreCohesionControl::createCurvesFromAddresses()
{
    using namespace std;
    // Fetch the selected constraints
    auto sel = currentDocument()->
                 selectionStack().
                   currentSelection();

    QList<const ConstraintModel*> selected_constraints;
    for(auto obj : sel)
    {
        if(auto cst = dynamic_cast<const ConstraintModel*>(obj))
            if(cst->selection.get())
                selected_constraints.push_back(cst);
    }

    // Fetch the selected DeviceExplorer elements
    auto device_explorer = currentDocument()->findChild<DeviceExplorerModel*>("DeviceExplorerModel");
    auto addresses = device_explorer->selectedIndexes();

    MacroCommandDispatcher macro{new CreateCurvesFromAddressesInConstraints,
                                 currentDocument()->commandStack()};
    for(auto& constraint : selected_constraints)
    {
        QList<Address> l;
        for(auto& index : addresses)
        {
            l.push_back(DeviceExplorer::addressFromModelIndex(index));
        }

        // TODO skip the ones that can't send messages or aren't int / double / float
        auto cmd = new CreateCurvesFromAddresses {iscore::IDocument::path(*constraint), l};
        macro.submitCommand(cmd);
    }

    macro.commit();
}

void IScoreCohesionControl::interpolateStates()
{
    using namespace std;
    // Fetch the selected constraints
    auto sel = currentDocument()->
                 selectionStack().
                   currentSelection();

    QList<const ConstraintModel*> selected_constraints;
    for(auto obj : sel)
    {
        // TODO replace with a virtual Element::type() which will be faster.
        if(auto cst = dynamic_cast<const ConstraintModel*>(obj))
        {
            if(cst->selection.get() && dynamic_cast<ScenarioModel*>(cst->parent()))
            {
                selected_constraints.push_back(cst);
            }
        }
    }

    // For each constraint, interpolate between the states in its start event and end event.

    // TODO maybe template it instead?
    MacroCommandDispatcher macro{new InterpolateMacro,
                                 currentDocument()->commandStack()};
    // They should all be in the same scenario so we can select the first.
    ScenarioModel* scenar =
            selected_constraints.empty()
            ? nullptr
            : dynamic_cast<ScenarioModel*>(selected_constraints.first()->parent());

    auto checkType = [] (const QVariant& var) {
        QMetaType::Type t = static_cast<QMetaType::Type>(var.type());
        return t == QMetaType::Int
            || t == QMetaType::Float
            || t == QMetaType::Double;
    };
    for(auto& constraint : selected_constraints)
    {
        const auto& startState = scenar->state(constraint->startState());
        const auto& endState = scenar->state(constraint->endState());

        iscore::MessageList startMessages = startState.messages().flatten();
        iscore::MessageList endMessages = endState.messages().flatten();

        for(auto& message : startMessages)
        {
            if(!checkType(message.value.val))
                continue;

            auto it = std::find_if(begin(endMessages),
                                   end(endMessages),
                         [&] (const Message& arg) { return message.address == arg.address; });

            if(it != end(endMessages))
            {
                if(!checkType((*it).value.val))
                    continue;

                auto has_existing_curve = std::find_if(
                            constraint->processes.begin(),
                            constraint->processes.end(),
                            [&] (const Process& proc) {
                   auto ptr = dynamic_cast<const AutomationModel*>(&proc);
                   if(ptr && ptr->address() == message.address)
                       return true;
                   return false;
                });

                if(has_existing_curve != constraint->processes.end())
                    continue;

                auto cmd = new CreateCurveFromStates{
                           iscore::IDocument::path(*constraint),
                           message.address,
                           message.value.val.toDouble(),
                           (*it).value.val.toDouble()};
                macro.submitCommand(cmd);
            }
        }
    }

    macro.commit();
}
#include "Process/Temporal/StateMachines/ScenarioPoint.hpp"
#include "Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp"

#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include "Curve/CurveModel.hpp"
#include "Commands/Scenario/Creations/CreateTimeNode_Event_State.hpp"
#include "Commands/Scenario/Creations/CreateConstraint_State_Event_TimeNode.hpp"
#include "Commands/Constraint/AddProcessToConstraint.hpp"
#include "Commands/Scenario/Displacement/MoveNewEvent.hpp"
#include "Commands/Scenario/Displacement/MoveEvent.hpp"
#include "Curve/Commands/UpdateCurve.hpp"
#include "Curve/Segment/Linear/LinearCurveSegmentModel.hpp"
// TODO moveme
class Record : public iscore::AggregateCommand
{
    ISCORE_COMMAND_DECL("IScoreCohesionControl", "Record", "Record")
    public:
        Record():
            AggregateCommand{factoryName(),
                             commandName(),
                             description()}
        { }

// TODO I require a special undo too
};

struct RecordData
{
        AddProcessToConstraint* addProcCmd{};

        CurveModel& curveModel;
        double min{};
        double max{};
        double initVal{};
};

class RecordManager : public QObject
{
        std::unique_ptr<QuietMacroCommandDispatcher> m_dispatcher;
        ListeningState m_listening;
        DeviceExplorerModel* m_explorer{};

        QTimer m_recordTimer;

        std::unordered_map<
                iscore::Address,
                RecordData
        > proc_cmds;
    public:
        RecordManager()
        {
            m_recordTimer.setInterval(8);
        }

        void stopRecording()
        {
            m_recordTimer.stop();
            // Commit

            m_explorer->deviceModel().resumeListening(m_listening);
        }

        void initRecording(ScenarioModel& scenar, ScenarioPoint pt)
        {
            auto& doc = *iscore::IDocument::documentFromObject(scenar);
            //// Device tree management ////

            // Get all the selected nodes
            m_explorer = doc.findChild<DeviceExplorerModel*>("DeviceExplorerModel");
            auto indices = m_explorer->selectedIndexes();

            // Disable listening for everything
            m_listening = m_explorer->deviceModel().pauseListening();

            // First get the addresses to listen.
            std::vector<std::vector<iscore::Address>> addresses_vec;
            for(auto& index : indices)
            {
                // TODO use address settings instead.
                auto addr = DeviceExplorer::addressFromModelIndex(index);
                // TODO shall we check if the address is in, out, recordable ?
                // Recording an automation of strings would actually have a meaning
                // here (for instance recording someone typing).

                // We sort the addresses by device to optimize.
                auto dev_it = std::find_if(addresses_vec.begin(),
                                           addresses_vec.end(),
                                           [&] (const auto& vec)
                { return vec.front().device == addr.device; });

                if(dev_it != addresses_vec.end())
                {
                    dev_it->push_back(addr);
                }
                else
                {
                    addresses_vec.push_back({addr});
                }
            }

            if(addresses_vec.empty())
                return;

            m_dispatcher = std::make_unique<QuietMacroCommandDispatcher>(new Record, doc.commandStack());

            //// Initial commands ////

            // Get the clicked point in scenario and create a state + constraint + state there
            // Create an automation + a rack + a slot + process views for all automations.
            qDebug() << "Creating at" << pt.date << pt.y;
            pt.date = TimeValue::fromMsecs(10000);
            auto default_end_date = pt.date + TimeValue::fromMsecs(1000);
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

            //// Creation of the curves ////
            for(const auto& vec : addresses_vec)
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


                    // Don't forget to put them all in the dispatcher at the end
                    // TODO fetch min / max from AddressSettings
                    // TODO fetch current value from AddressSettings
                    proc_cmds.insert(std::make_pair(addr, RecordData{cmd_proc, autom.curve(), -1., 1., 0.}));
                }
            }

            //// Setup listening on the curves ////
            for(const auto& vec : addresses_vec)
            {
                auto& dev = m_explorer->deviceModel().list().device(vec.front().device);

                dev.replaceListening(vec);
                // Add a custom callback.
                connect(&dev, &DeviceInterface::valueUpdated,
                        this, [=] (const iscore::Address& addr, const iscore::Value& val) {

                    bool ok = false;
                    double newval = val.val.toDouble(&ok);
                    if(!ok)
                        return;

                    qDebug() << addr.toString() << newval;
                });
            }

            //// Start the record timer ////
            auto start_time_pt = std::chrono::steady_clock::now();
            auto current_time_pt = std::make_shared<std::chrono::steady_clock::time_point>();
            connect(&m_recordTimer, &QTimer::timeout,
                    this, [=] () {
                *current_time_pt = std::chrono::steady_clock::now();

                // Move end event by the current duration.
                int msecs = std::chrono::duration_cast<std::chrono::milliseconds>(*current_time_pt - start_time_pt).count();
                cmd_move->update(
                            Path<ScenarioModel>{},
                            Id<ConstraintModel>{},
                            cmd_end->createdEvent(),
                            TimeValue::fromMsecs(msecs),
                            0,
                            true);

                cmd_move->redo();

            });

            m_recordTimer.start();
            // TODO don't forget to set min/max/address at the end.

            // At each tick, resize the constraint
            // At each value received, resize the process (increase duration mode) to match the current value,
            // and add a curve segment from the last to the current.
            // Also, rescale with min / max values.
            // So, if min is -3 and max is 5, the lowest point will be at 0 and the biggest at 1,
            // with the min and max set in a relevant way.
            // i.e., each time a new point is > max or < min, rescale everything in the
            // automation.

            // On stop, create the relevant command with the recorded automation by creating UpdateCurves commands.
            // Push everything in quiet mode.

            // Note : create a special curve command with an addNewPoint() method ?

            /*
            auto update_proc = new UpdateCurve(iscore::IDocument::path(autom.curve(), {}));
            */

        }
};

void IScoreCohesionControl::record(ScenarioModel& scenar, ScenarioPoint pt)
{
    qDebug() << Q_FUNC_INFO;
    RecordManager* mgr = new RecordManager;
    mgr->initRecording(scenar, pt);
/*

    // Time keeping
    auto start_time_pt = std::chrono::steady_clock::now();
    auto current_time_pt = std::make_shared<std::chrono::steady_clock::time_point>();
    // Enable "record" listening for the selected nodes
    // and set a relevant callback
    for(const auto& vec : addresses_vec)
    {
        auto& dev = device_explorer->deviceModel().list().device(vec.front().device);

        dev.replaceListening(vec);
        // Add a custom callback.
        connect(&dev, &DeviceInterface::valueUpdated,
                this, [=] (const iscore::Address& addr, const iscore::Value& val) {

            bool ok = false;
            double newval = val.val.toDouble(&ok);
            if(!ok)
                return;

            qDebug() << addr << newval;

            auto it = proc_cmds->find(addr);
            ISCORE_ASSERT(it != proc_cmds->end());

            // We go from zero and add segments after segments.
            CurveModel* curve = std::get<1>(it->second);
            int num = curve->segments().size();
            Id<CurveSegmentModel> id_cur{num};
            auto seg = new LinearCurveSegmentModel{id_cur, curve};
            if(num > 0)
            {
                Id<CurveSegmentModel> id_prev{num - 1};
                seg->setPrevious(id_prev);

                auto& prev_seg = curve->segments().at(id_prev);
                prev_seg.setFollowing(id_cur);
                seg->setStart(prev_seg.end());
            }
            else
            {
                auto& startval = std::get<2>(std::get<2>(it->second));
                seg->setStart({0, startval});
            }

            // Rescale
            auto& min = std::get<0>(std::get<2>(it->second));
            auto& max = std::get<1>(std::get<2>(it->second));
            if(newval < min)
            {
                // scale everything "reverse-down" and update min
                min = newval;
                // TODO TODO TODO
            }

            if(newval > max)
            {
                // scale everything down and update max
                for(CurveSegmentModel& segment : curve->segments())
                {
                    auto start = segment.start();
                    auto end = segment.end();
                    // TODO div by zero incoming
                    segment.setStart({start.x(), start.y() * (max / newval)});
                    segment.setEnd({end.x(), end.y() * (max / newval)});
                }
                max = newval;
            }

            // Get elapsed time since last tick and rescale everything
            // relating to the new duration.
            // Keep this in a struct too. (The big thing in unordered_map should become a struct).
            // And make a class from this horrible method).

            seg->setEnd({1, newval / max});

            curve->insertSegment(seg);
        });
    }
    connect(&m_recordTimer, &QTimer::timeout,
            this, [&] () {
        *current_time_pt = std::chrono::steady_clock::now();

        // Move end event by the current duration.
        qDebug() << "yo dawg" << std::chrono::duration_cast<std::chrono::milliseconds>(*current_time_pt - start_time_pt).count()

    });

    m_recordTimer.start();*/
    // TODO don't forget to set min/max/address at the end.

    // At each tick, resize the constraint
    // At each value received, resize the process (increase duration mode) to match the current value,
    // and add a curve segment from the last to the current.
    // Also, rescale with min / max values.
    // So, if min is -3 and max is 5, the lowest point will be at 0 and the biggest at 1,
    // with the min and max set in a relevant way.
    // i.e., each time a new point is > max or < min, rescale everything in the
    // automation.

    // On stop, create the relevant command with the recorded automation by creating UpdateCurves commands.
    // Push everything in quiet mode.

    // Note : create a special curve command with an addNewPoint() method ?

    /*
    auto update_proc = new UpdateCurve(iscore::IDocument::path(autom.curve(), {}));
    */

    }

    void IScoreCohesionControl::stopRecord(){}


    void IScoreCohesionControl::snapshotParametersInStates()
    {
        using namespace std;
        // Fetch the selected events
    auto sel = currentDocument()->
                 selectionStack().
                   currentSelection();

    QList<const StateModel*> selected_states;
    for(auto obj : sel)
    {
        if(auto st = dynamic_cast<const StateModel*>(obj))
            if(st->selection.get()) // TODO this should not be necessary?
                selected_states.push_back(st);
    }

    // Fetch the selected DeviceExplorer elements
    auto device_explorer = currentDocument()->findChild<DeviceExplorerModel*>("DeviceExplorerModel");
    auto indexes = device_explorer->selectedIndexes();

    MessageList messages;
    for(auto& index : indexes)
    {
        auto m = DeviceExplorer::messageFromModelIndex(index);
        if(m != iscore::Message{})
            messages.push_back(m);
    }

    if(messages.empty())
        return;

    MacroCommandDispatcher macro{new CreateStatesFromParametersInEvents,
                                 currentDocument()->commandStack()};
    for(auto& state : selected_states)
    {
        auto cmd = new AddMessagesToModel{
                   iscore::IDocument::path(state->messages()),
                   messages};
        macro.submitCommand(cmd);
    }

    macro.commit();
}
