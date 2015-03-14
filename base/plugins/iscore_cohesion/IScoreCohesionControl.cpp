#include "IScoreCohesionControl.hpp"
#include <iscore/menu/MenuInterface.hpp>
#include <core/presenter/MenubarManager.hpp>
#include <QApplication>

#include "../scenario/source/Document/Constraint/ViewModels/AbstractConstraintViewModel.hpp"
#include "../scenario/source/Document/Constraint/ViewModels/AbstractConstraintPresenter.hpp"
#include "../scenario/source/Document/Constraint/ConstraintModel.hpp"
#include "../scenario/source/Document/Event/EventModel.hpp"
#include "../scenario/source/Document/BaseElement/BaseElementModel.hpp"
#include "../scenario/source/Document/BaseElement/BaseElementPresenter.hpp"
#include "../device_explorer/DeviceInterface/DeviceExplorerInterface.hpp"
#include "../device_explorer/Panel/DeviceExplorerModel.hpp"

#include "Commands/CreatesStatesFromParameters.hpp"
#include "Commands/CreateStatesFromParametersInEvents.hpp"

#include "iscore/document/DocumentInterface.hpp"
#include "iscore/command/OngoingCommandManager.hpp"
#include "iscore/selection/SelectionStack.hpp"
#include <Commands/CreateCurvesFromAddresses.hpp>
#include <Commands/CreateCurvesFromAddressesInConstraints.hpp>
#include "FakeEngine.hpp"
#include <source/Control/OldFormatConversion.hpp>
#include <source/Document/BaseElement/BaseElementModel.hpp>
#include <QTemporaryFile>

// TODO : snapshot : doit être un mode d'édition particulier
// on enregistre l'état précédent et on crée les courbes correspondantes
#include <QAction>
using namespace iscore;
IScoreCohesionControl::IScoreCohesionControl(QObject* parent) :
    iscore::PluginControlInterface {"IScoreCohesionControl", parent}
{

    connect(&m_engine, &FakeEngine::currentTimeChanged,
            this, &IScoreCohesionControl::on_currentTimeChanged);
}

void IScoreCohesionControl::populateMenus(iscore::MenubarManager* menu)
{
    // If there is the Curve plug-in, the Device Explorer, and the Scenario plug-in,
    // We can add an option in the menu to generate curves from the selected addresses
    // in the current constraint.
    QAction* curvesFromAddresses = new QAction {tr("Create Curves"), this};
    connect(curvesFromAddresses, &QAction::triggered,
            this,				 &IScoreCohesionControl::createCurvesFromAddresses);

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::EditMenu,
                                       curvesFromAddresses);


    QAction* snapshot = new QAction {tr("Snapshot in Event"), this};
    connect(snapshot, &QAction::triggered,
            this,     &IScoreCohesionControl::snapshotParametersInEvents);

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::EditMenu,
                                       snapshot);


    QAction* play = new QAction {tr("Play in 0.2 engine"), this};
    connect(play, &QAction::triggered,
            [&] ()
    {
        m_scoreFile.close();
        if(m_scoreFile.open())
        {
            auto data = JSONToZeroTwo(currentDocument()->saveAsJson());

            m_scoreFile.write(data.toLatin1().constData(), data.size());
            m_scoreFile.flush();
            m_engine.runScore(m_scoreFile.fileName());
        }
    });

    menu->insertActionIntoToplevelMenu(ToplevelMenuElement::EditMenu,
                                       play);
}

SerializableCommand* IScoreCohesionControl::instantiateUndoCommand(const QString& name, const QByteArray& data)
{
    iscore::SerializableCommand* cmd{};
    if(false);
    else if(name == "CreateCurvesFromAddresses") cmd = new CreateCurvesFromAddresses;
    else if(name == "CreateCurvesFromAddressesInConstraints") cmd = new CreateCurvesFromAddressesInConstraints;
    else if(name == "CreateStatesFromParametersInEvents") cmd = new CreateStatesFromParametersInEvents;;

    if(!cmd)
    {
        qDebug() << Q_FUNC_INFO << "Warning : command" << name << "received, but it could not be read.";
        return nullptr;
    }

    cmd->deserialize(data);
    return cmd;
}

#include <core/document/DocumentModel.hpp>
#include <Document/BaseElement/BaseElementPresenter.hpp>
void IScoreCohesionControl::on_currentTimeChanged(double t)
{
    auto bep = static_cast<BaseElementPresenter*>(currentDocument()->presenter()->presenterDelegate());
    bep->setProgressBarTime(TimeValue(std::chrono::milliseconds((uint32_t)t)));
}

void IScoreCohesionControl::createCurvesFromAddresses()
{
    using namespace std;
    // Fetch the selected constraints
    auto sel = currentDocument()->
                 selectionStack().
                   currentSelection();

    QList<ConstraintModel*> selected_constraints;
    for(auto obj : sel)
    {
        if(auto cst = dynamic_cast<ConstraintModel*>(obj))
            if(cst->selection.get())
                selected_constraints.push_back(cst);
    }

    // Fetch the selected DeviceExplorer elements
    auto device_explorer = DeviceExplorer::getModel(currentDocument());
    auto addresses = device_explorer->selectedIndexes();

    MacroCommandDispatcher macro(new CreateCurvesFromAddressesInConstraints,
                                 currentDocument()->commandStack(),
                                 nullptr);
    for(auto& constraint : selected_constraints)
    {
        QStringList l;
        for(auto& index : addresses)
        {
            l.push_back(DeviceExplorer::addressFromModelIndex(index));
        }

        auto cmd = new CreateCurvesFromAddresses {iscore::IDocument::path(constraint), l};
        macro.submitCommand(cmd);
    }

    macro.commit();
}


#include "../scenario/source/Commands/Event/AddStateToEvent.hpp"
void IScoreCohesionControl::snapshotParametersInEvents()
{
    using namespace std;
    // Fetch the selected events
    auto sel = currentDocument()->
                 selectionStack().
                   currentSelection();

    QList<EventModel*> selected_events;
    for(auto obj : sel)
    {
        if(auto ev = dynamic_cast<EventModel*>(obj))
            if(ev->selection.get()) // TODO this should not be necessary?
                selected_events.push_back(ev);
    }

    // Fetch the selected DeviceExplorer elements
    auto device_explorer = DeviceExplorer::getModel(currentDocument());
    auto indexes = device_explorer->selectedIndexes();

    MessageList messages;
    for(auto& index : indexes)
        messages.push_back(DeviceExplorer::messageFromModelIndex(index));


    MacroCommandDispatcher macro(new CreateStatesFromParametersInEvents,
                                 currentDocument()->commandStack(),
                                 nullptr);
    for(auto& event : selected_events)
    {
        auto cmd = new Scenario::Command::AddStateToEvent{
                              iscore::IDocument::path(event), messages};
        macro.submitCommand(cmd);
    }

    macro.commit();
}
