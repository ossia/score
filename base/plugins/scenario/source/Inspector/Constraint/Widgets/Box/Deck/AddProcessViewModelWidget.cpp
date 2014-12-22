#include "AddProcessViewModelWidget.hpp"

#include "DeckInspectorSection.hpp"
#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"
#include "ProcessInterface/ProcessViewModelInterface.hpp"

#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QLabel>
#include <QtWidgets/QInputDialog>

AddProcessViewModelWidget::AddProcessViewModelWidget(DeckInspectorSection* parentDeck):
	QWidget{parentDeck}
{
	QHBoxLayout* layout = new QHBoxLayout;
	layout->setContentsMargins(0, 0, 0 ,0);
	this->setLayout(layout);

	// Button
	QToolButton* addButton = new QToolButton;
	addButton->setText ("+");

	// Text
	auto addText = new QLabel("Add Process View");
	addText->setStyleSheet (QString ("text-align : left;") );

	layout->addWidget(addButton);
	layout->addWidget(addText);

	connect(addButton, &QToolButton::pressed,
			[=] ()
	{
		QStringList available_models;

		// TODO put this part in DeckModel if it is required elsewhere.
		// 1. List the processes in the model.
		auto shared_process_list = parentDeck->model()->parentConstraint()->processes();

		// 2. List the processes that already have a view in this deck
		auto already_displayed_processes = parentDeck->model()->processViewModels();

		// 3. Compute the difference
		for(auto& process : shared_process_list)
		{
			auto it = std::find_if(std::begin(already_displayed_processes),
								   std::end(already_displayed_processes),
								   [&process] (ProcessViewModelInterface* pvm)
			{ return pvm->sharedProcessId() == process->id(); });

			if(it == std::end(already_displayed_processes))
			{
				available_models += QString::number(process->id());
			}
		}

		// 4. Present a dialog with the availble id's
		if(available_models.size() > 0)
		{
			bool ok = false;
			auto process_name = QInputDialog::getItem(this,
													  QObject::tr("Choose a process id"),
													  QObject::tr("Choose a process id"),
													  available_models,
													  0,
													  false,
													  &ok);

			if(ok)
				parentDeck->createProcessViewModel(process_name.toInt());
		}
	} );
}
