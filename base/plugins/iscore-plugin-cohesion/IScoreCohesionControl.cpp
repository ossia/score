#include "IScoreCohesionControl.hpp"
#include <QApplication>

#include "Document/Constraint/ViewModels/ConstraintViewModel.hpp"
#include "Document/Constraint/ViewModels/ConstraintPresenter.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Event/EventModel.hpp"
#include "Document/BaseElement/BaseElementPresenter.hpp"
#include "Process/ScenarioModel.hpp"
#include "Commands/Event/AddStateToEvent.hpp"
#include "RecordManager.hpp"
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

#include "Process/Temporal/StateMachines/ScenarioPoint.hpp"
#include "Plugin/DocumentPlugin/DeviceDocumentPlugin.hpp"

#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include "Curve/CurveModel.hpp"

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
    connect(scen, &ScenarioControl::stopRecording,
            this, &IScoreCohesionControl::stopRecord);



    auto acts = scen->actions();
    for(const auto& act : acts)
    {
        if(act->objectName() == "Stop")
        {
            connect(act, &QAction::triggered,
                    this, [&] {
                stopRecord();
            });
        }
    }

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


struct IScoreCohesionCommandFactory
{
        static CommandGeneratorMap map;
};

CommandGeneratorMap IScoreCohesionCommandFactory::map;

void IScoreCohesionControl::setupCommands()
{
    boost::mpl::for_each<
            boost::mpl::list<
            CreateCurvesFromAddresses,
            CreateCurvesFromAddressesInConstraints,
            InterpolateMacro,
            Record,
            CreateStatesFromParametersInEvents
            >,
            boost::type<boost::mpl::_>
            >(CommandGeneratorMapInserter<IScoreCohesionCommandFactory>());
}

SerializableCommand* IScoreCohesionControl::instantiateUndoCommand(const QString& name, const QByteArray& data)
{
    return PluginControlInterface::instantiateUndoCommand<IScoreCohesionCommandFactory>(name, data);
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

void IScoreCohesionControl::record(ScenarioModel& scenar, ScenarioPoint pt)
{
    m_recManager = std::make_unique<RecordManager>();
    m_recManager->recordInNewBox(scenar, pt);

}

void IScoreCohesionControl::stopRecord()
{
    if(m_recManager)
    {
        m_recManager->stopRecording();
        m_recManager.release();
    }
}


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
