#include "IScoreCohesionControl.hpp"
#include <interface/plugincontrol/MenuInterface.hpp>
#include <core/presenter/MenubarManager.hpp>
#include <QApplication>

#include "../scenario/source/Document/Constraint/ViewModels/AbstractConstraintViewModel.hpp"
#include "../scenario/source/Document/Constraint/ViewModels/AbstractConstraintPresenter.hpp"
#include "../scenario/source/Document/Constraint/ConstraintModel.hpp"
#include "../scenario/source/Document/BaseElement/BaseElementModel.hpp"
#include "../scenario/source/Document/BaseElement/BaseElementPresenter.hpp"
#include "../device_explorer/DeviceInterface/DeviceExplorerInterface.hpp"
#include "../device_explorer/Panel/DeviceExplorerModel.hpp"

#include "core/interface/document/DocumentInterface.hpp"
#include "core/presenter/command/OngoingCommandManager.hpp"
#include "core/interface/selection/SelectionStack.hpp"
#include <Commands/CreateCurvesFromAddresses.hpp>
#include <Commands/CreateCurvesFromAddressesInConstraints.hpp>
#include "FakeEngine.hpp"
#include <source/Control/OldFormatConversion.hpp>
#include <source/Document/BaseElement/BaseElementModel.hpp>
#include <QTemporaryFile>


#include <QAction>
using namespace iscore;
IScoreCohesionControl::IScoreCohesionControl(QObject* parent) :
    iscore::PluginControlInterface {"IScoreCohesionControl", parent}
{

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

    // If there is the Curve plug-in, the Device Explorer, and the Scenario plug-in,
    // We can add an option in the menu to generate curves from the selected addresses
    // in the current constraint.
    QAction* play = new QAction {tr("Play in 0.2 engine"), this};
    connect(play, &QAction::triggered,
            [&] ()
    {
        QTemporaryFile f;

        if(f.open())
        {
            auto& doc = IDocument::modelDelegate<BaseElementModel>(currentDocument());
            auto data = JSONToZeroTwo(doc.toJson());

            f.write(data.toLatin1().constData(), data.size());
            f.flush();
            runScore(f.fileName());
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

    if(!cmd)
    {
        qDebug() << Q_FUNC_INFO << "Warning : command" << name << "received, but it could not be read.";
        return nullptr;
    }

    cmd->deserialize(data);
    return cmd;
}

void IScoreCohesionControl::createCurvesFromAddresses()
{
    using namespace std;
    // TODO this should take a document as argument.
    // Fetch the selected constraints
    SelectionStack& selectionStack = currentDocument()->selectionStack();
    auto sel = selectionStack.currentSelection();
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
