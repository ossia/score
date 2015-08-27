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

// TODO Refactor in order to use the Node data structure instead.
#include <DeviceExplorer/../Plugin/Panel/DeviceExplorerModel.hpp>
#include <DeviceExplorer/Node/DeviceExplorerNode.hpp>

#include "Commands/CreateStatesFromParametersInEvents.hpp"

#include <Commands/CreateCurvesFromAddresses.hpp>
#include <Commands/CreateCurvesFromAddressesInConstraints.hpp>
#include "Commands/CreateCurveFromStates.hpp"
#include <core/document/DocumentPresenter.hpp>
#include <iscore/command/Dispatchers/MacroCommandDispatcher.hpp>
#include <source/Control/OldFormatConversion.hpp>
#include <source/Document/BaseElement/BaseElementModel.hpp>
#include <QKeySequence>
#include <iscore/command/CommandGeneratorMap.hpp>

#include <core/document/DocumentModel.hpp>
#include <QToolBar>

using namespace iscore;
IScoreCohesionControl::IScoreCohesionControl(Presenter* pres) :
    iscore::PluginControlInterface {pres, "IScoreCohesionControl", nullptr}
{
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

        auto cmd = new CreateCurvesFromAddresses {iscore::IDocument::safe_path(*constraint), l};
        macro.submitCommand(cmd);
    }

    macro.commit();
}

void IScoreCohesionControl::interpolateStates()
{
    ISCORE_TODO;
    /*
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

    for(auto& constraint : selected_constraints)
    {
        // TODO state collapsing if twice the same message ?
        // Check the states similar between its start and end event
        const auto& startState = scenar->state(constraint->startState());
        const auto& endState = scenar->state(constraint->endState());

        QList<Message> startMessages;
        for(const auto& state : startState.states())
        {
            if(state.data().canConvert<Message>())
            {
                startMessages.push_back(state.data().value<Message>());
            }
            else if(state.data().canConvert<MessageList>())
            {
                startMessages += state.data().value<MessageList>();
            }
        }

        QList<Message> endMessages;
        for(auto& state : endState.states())
        {
            if(state.data().canConvert<Message>())
            {
                endMessages.push_back(state.data().value<Message>());
            }
            else if(state.data().canConvert<MessageList>())
            {
                endMessages += state.data().value<MessageList>();
            }
        }

        for(auto& message : startMessages)
        {
            auto it = std::find_if(begin(endMessages),
                                   end(endMessages),
                         [&] (const Message& arg) { return message.address == arg.address; });

            if(it != end(endMessages))
            {
                auto cmd = new CreateCurveFromStates{
                           iscore::IDocument::path(constraint),
                           message.address,
                           message.value.val.toDouble(),
                           (*it).value.val.toDouble()};
                macro.submitCommand(cmd);
            }
        }
    }

    macro.commit();
    */
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
        messages.push_back(DeviceExplorer::messageFromModelIndex(index));

    MacroCommandDispatcher macro{new CreateStatesFromParametersInEvents,
                                 currentDocument()->commandStack()};
    for(auto& state : selected_states)
    {
        auto cmd = new Scenario::Command::AddStateToStateModel{
                   iscore::IDocument::safe_path(*state),
                   iscore::StatePath{}, // Make it child of the root node
                   iscore::StateData(std::move(messages), "NewState"),
                   -1};
        macro.submitCommand(cmd);
    }

    macro.commit();
}
