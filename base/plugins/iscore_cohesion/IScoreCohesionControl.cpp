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
#include <Commands/CreateCurvesFromAddresses.hpp>
#include "FakeEngine.hpp"

#include <QAction>
using namespace iscore;
IScoreCohesionControl::IScoreCohesionControl(QObject *parent):
	iscore::PluginControlInterface{"IScoreCohesionControl", parent}
{

}

void IScoreCohesionControl::populateMenus(iscore::MenubarManager* menu)
{
	// If there is the Curve plug-in, the Device Explorer, and the Scenario plug-in,
	// We can add an option in the menu to generate curves from the selected addresses
	// in the current constraint.
	QAction* curvesFromAddresses = new QAction{tr("Create Curves"), this};
	connect(curvesFromAddresses, &QAction::triggered,
			this,				 &IScoreCohesionControl::createCurvesFromAddresses);

	menu->insertActionIntoToplevelMenu(ToplevelMenuElement::EditMenu,
									   curvesFromAddresses);

	// If there is the Curve plug-in, the Device Explorer, and the Scenario plug-in,
	// We can add an option in the menu to generate curves from the selected addresses
	// in the current constraint.
	QAction* play = new QAction{tr("Play in 0.2 engine"), this};
	connect(play, &QAction::triggered, &FakeEngineExecute);

	menu->insertActionIntoToplevelMenu(ToplevelMenuElement::EditMenu,
									   play);
}

SerializableCommand *IScoreCohesionControl::instantiateUndoCommand(QString name, QByteArray data)
{
	return nullptr;
}

void IScoreCohesionControl::createCurvesFromAddresses()
{
	// TODO use current document.

	// Fetch the selected constraints
	auto pres = qApp->findChild<BaseElementPresenter*>("BaseElementPresenter");
	auto constraints = pres->findChildren<AbstractConstraintPresenter*>();
	QList<ConstraintModel*> selected_constraints;
	for(AbstractConstraintPresenter* constraint : constraints)
	{
		if(constraint->isSelected())
			selected_constraints.push_back(constraint->abstractConstraintViewModel()->model());
	}

	// Fetch the selected DeviceExplorer elements
	auto device_explorer = DeviceExplorer::getModel(pres->model());
	auto addresses = device_explorer->selectedIndexes();

	for(auto& constraint : selected_constraints)
	{
		QStringList l;
		for(auto& index : addresses)
		{
			l.push_back(DeviceExplorer::addressFromModelIndex(index));
		}

		auto cmd = new CreateCurvesFromAddresses{iscore::IDocument::path(constraint), l};
		submitCommand(cmd);
	}
}
