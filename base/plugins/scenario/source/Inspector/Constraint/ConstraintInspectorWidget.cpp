#include "ConstraintInspectorWidget.hpp"

#include "Widgets/AddSharedProcessWidget.hpp"
#include "Widgets/BoxWidget.hpp"
#include "Widgets/DurationSectionWidget.hpp"
#include "Widgets/Box/BoxInspectorSection.hpp"

#include "Document/Constraint/ConstraintModel.hpp"
#include "Document/Constraint/ViewModels/Temporal/TemporalConstraintViewModel.hpp"
#include "Document/Constraint/Box/BoxModel.hpp"
#include "Document/Constraint/Box/Deck/DeckModel.hpp"
#include "Commands/Constraint/AddProcessToConstraint.hpp"
#include "Commands/Constraint/AddProcessViewInNewDeck.hpp"
#include "Commands/Constraint/AddBoxToConstraint.hpp"
#include "Commands/Scenario/ShowBoxInViewModel.hpp"
#include "Commands/Scenario/HideBoxInViewModel.hpp"
#include "Commands/Metadata/ChangeElementLabel.hpp"
#include "Commands/Metadata/ChangeElementName.hpp"
#include "Commands/Metadata/ChangeElementComments.hpp"
#include "Commands/Metadata/ChangeElementColor.hpp"
#include "ProcessInterface/ProcessSharedModelInterface.hpp"

#include "Inspector/MetadataWidget.hpp"
#include <InspectorInterface/InspectorSectionWidget.hpp>
#include <InspectorControl.hpp>

#include <tools/ObjectPath.hpp>

#include <QFrame>
#include <QLineEdit>
#include <QLayout>
#include <QFormLayout>
#include <QWidget>
#include <QToolButton>
#include <QPushButton>
#include <QApplication>
#include <QLabel>

using namespace Scenario::Command;


class Separator : public QFrame
{
	public:
		Separator(QWidget* parent):
			QFrame{parent}
		{
			setFrameShape(QFrame::HLine);
			setLineWidth(2);
		}
};

#include "Document/BaseElement/BaseElementPresenter.hpp"
ConstraintInspectorWidget::ConstraintInspectorWidget (TemporalConstraintViewModel* object, QWidget* parent) :
    InspectorWidgetBase (parent)
{
	setObjectName ("Constraint");
    setInspectedObject(object);
	m_currentConstraint = object;

	QPushButton* setAsDisplayedConstraint = new QPushButton{"Full view", this};
	connect(setAsDisplayedConstraint, &QPushButton::clicked,
			[this] ()
	{
		auto base = qApp->findChild<BaseElementPresenter*>("BaseElementPresenter");
		base->setDisplayedConstraint(this->model());
	});
	m_properties.push_back(setAsDisplayedConstraint);

    // Events

    QWidget* eventWid = new QWidget{};
    QFormLayout* eventLay = new QFormLayout{eventWid};
    QPushButton* start = new QPushButton{};
    start->setFlat(true);
	if(object->model()->startEvent().val())
	{
		start->setText(QString::number(*object->model()->startEvent().val()));
	}
	else
	{
		start->setText(tr("None"));
	}

    QPushButton* end = new QPushButton{};
    end->setFlat(true);
	if(object->model()->startEvent().val())
	{
		end->setText(QString::number(*object->model()->endEvent().val()));
	}
	else
	{
		end->setText(tr("None"));
	}

    eventLay->addRow("Start Event", start);
    eventLay->addRow("End Event", end);

    m_properties.push_back(eventWid);

    // Separator
    m_properties.push_back(new Separator{this});


	m_durationSection = new DurationSectionWidget{this};
	m_properties.push_back(m_durationSection);

	// Separator
	m_properties.push_back(new Separator{this});

	// Processes
	m_processSection = new InspectorSectionWidget ("Processes", this);
	m_processSection->setObjectName("Processes");

	m_properties.push_back(m_processSection);
	m_properties.push_back(new AddSharedProcessWidget{this});

	// Separator
	m_properties.push_back(new Separator{this});

	// Boxes
	m_boxSection = new InspectorSectionWidget{"Boxes", this};
	m_boxSection->setObjectName("Boxes");

	m_boxWidget = new BoxWidget{this};

	m_properties.push_back(m_boxSection);
	m_properties.push_back(m_boxWidget);

	// Display data
	updateSectionsView (areaLayout(), m_properties);
	areaLayout()->addStretch(1);

    // metadata
    m_metadata = new MetadataWidget(&object->model()->metadata, this);
    m_metadata->setType("Constraint");
    addHeader(m_metadata);

	updateDisplayedValues(object);

    connect(m_metadata,     &MetadataWidget::scriptingNameChanged,
            this,           &ConstraintInspectorWidget::on_scriptingNameChanged);

    connect(m_metadata,     &MetadataWidget::labelChanged,
            this,           &ConstraintInspectorWidget::on_labelChanged);

    connect(m_metadata,     &MetadataWidget::commentsChanged,
            this,           &ConstraintInspectorWidget::on_commentsChanged);

    connect(m_metadata,      &MetadataWidget::colorChanged,
            this,           &ConstraintInspectorWidget::on_colorChanged);

//    connect(m_metadata,      &MetadataWidget::inspectPreviousElement,
//            m_currentConstraint,    &TemporalConstraintViewModel::inspectPreviousElement);

    connect(end,    &QPushButton::clicked,
            [=] ()
    {
        object->eventSelected(end->text());
    });

    connect(start, &QPushButton::clicked,
            [=] ()
    {
        object->eventSelected(start->text());
    });

}

TemporalConstraintViewModel*ConstraintInspectorWidget::viewModel() const
{ return m_currentConstraint; }

ConstraintModel* ConstraintInspectorWidget::model() const
{ return m_currentConstraint? m_currentConstraint->model() : nullptr; }

void ConstraintInspectorWidget::updateDisplayedValues (TemporalConstraintViewModel* constraint)
{
	// Cleanup the widgets
	for(auto& process : m_processesSectionWidgets)
	{
		m_processSection->removeContent(process);
	}
	m_processesSectionWidgets.clear();

	for(auto& box_pair : m_boxesSectionWidgets)
	{
		m_boxSection->removeContent(box_pair.second);
	}
	m_boxesSectionWidgets.clear();

	// Cleanup the connections
	for(auto& connection : m_connections)
		QObject::disconnect(connection);
	m_connections.clear();


	if (constraint != nullptr)
	{
		m_currentConstraint = constraint;
/*
		// Constraint settings
		setName (model()->metadata.name() );
		setColor (model()->metadata.color() );
		setComments (model()->metadata.comment() );
		setInspectedObject (m_currentConstraint);
		changeLabelType ("Constraint");
*/
		// Constraint interface
		m_connections.push_back(
					connect(model(),	&ConstraintModel::processCreated,
							this,		&ConstraintInspectorWidget::on_processCreated));
		m_connections.push_back(
					connect(model(),	&ConstraintModel::processRemoved,
							this,		&ConstraintInspectorWidget::on_processRemoved));
		m_connections.push_back(
					connect(model(),	&ConstraintModel::boxCreated,
							this,		&ConstraintInspectorWidget::on_boxCreated));
		m_connections.push_back(
					connect(model(),	&ConstraintModel::boxRemoved,
							this,		&ConstraintInspectorWidget::on_boxRemoved));

		// Processes
		for(ProcessSharedModelInterface* process : model()->processes())
		{
			displaySharedProcess(process);
		}

		// Box
		m_boxWidget->setModel(model());
		m_boxWidget->updateComboBox();

		for(BoxModel* box: model()->boxes())
		{
			setupBox(box);
		}
	}
	else
	{
		m_currentConstraint = nullptr;
		m_boxWidget->setModel(nullptr);
	}
}

void ConstraintInspectorWidget::createProcess(QString processName)
{
	auto cmd = new AddProcessToConstraint{
			   ObjectPath::pathFromObject("BaseElementModel",
										  model()),
			   processName};
	emit submitCommand(cmd);
}

void ConstraintInspectorWidget::createBox()
{
	auto cmd = new AddBoxToConstraint(
				   ObjectPath::pathFromObject(
					   "BaseConstraintModel",
					   model()));
    emit submitCommand(cmd);
}

void ConstraintInspectorWidget::createProcessViewInNewDeck(QString processName)
{
    auto cmd = new AddProcessViewInNewDeck(
                ObjectPath::pathFromObject(
                    "BaseConstraintModel",
                    model()),
                processName);
    emit submitCommand(cmd);
}

void ConstraintInspectorWidget::activeBoxChanged(QString box)
{
	// TODO mettre à jour l'inspecteur si la box affichée change (i.e. via une commande réseau).
	if(box == m_boxWidget->hiddenText)
	{
		if(viewModel()->isBoxShown())
		{
			auto cmd = new HideBoxInViewModel(viewModel());
			emit submitCommand(cmd);
		}
	}
	else
	{
		bool ok{};
		auto id = id_type<BoxModel>(box.toInt(&ok));

		if(ok)
		{
			auto cmd = new ShowBoxInViewModel(viewModel(), id);
			emit submitCommand(cmd);
		}
    }
}

void ConstraintInspectorWidget::on_scriptingNameChanged(QString newName)
{
    /*
    auto cmd = new ChangeElementName<ConstraintModel>( ObjectPath::pathFromObject(inspectedObject()),
                newName);

    submitCommand(cmd);
    //*/
}

void ConstraintInspectorWidget::on_labelChanged(QString newLabel)
{
    /*
    auto cmd = new ChangeElementLabel<ConstraintModel>( ObjectPath::pathFromObject(inspectedObject()),
                newLabel);

    submitCommand(cmd);
    //*/
}

void ConstraintInspectorWidget::on_commentsChanged(QString)
{

}

void ConstraintInspectorWidget::on_colorChanged(QColor)
{
//    auto cmd = new ChangeElementColor();

//    submitCommand(cmd);
}


void ConstraintInspectorWidget::displaySharedProcess(ProcessSharedModelInterface* process)
{
	InspectorSectionWidget* newProc = new InspectorSectionWidget (process->processName());
	auto widg = InspectorControl::getInspectorWidget(process);

	newProc->addContent(widg);

	m_processesSectionWidgets.push_back (newProc);
	m_processSection->addContent (newProc);

    connect(widg,   SIGNAL(createViewInNewDeck(QString)),
            this,   SLOT(createProcessViewInNewDeck(QString)));
}

void ConstraintInspectorWidget::setupBox(BoxModel* box)
{
	// Display the widget
	BoxInspectorSection* newBox = new BoxInspectorSection{QString{"Box.%1"}.arg(*box->id().val()),
								  box,
								  this};

	connect(newBox, &BoxInspectorSection::submitCommand,
			this,	&ConstraintInspectorWidget::submitCommand);

	m_boxesSectionWidgets[box->id()] = newBox;
	m_boxSection->addContent(newBox);
}


void ConstraintInspectorWidget::on_processCreated(QString processName, id_type<ProcessSharedModelInterface> processId)
{
	reloadDisplayedValues();
}

void ConstraintInspectorWidget::on_processRemoved(id_type<ProcessSharedModelInterface> processId)
{
	reloadDisplayedValues();
}


void ConstraintInspectorWidget::on_boxCreated(id_type<BoxModel> boxId)
{
	setupBox(model()->box(boxId));
	m_boxWidget->updateComboBox();
}

void ConstraintInspectorWidget::on_boxRemoved(id_type<BoxModel> boxId)
{
	auto ptr = m_boxesSectionWidgets[boxId];
	m_boxesSectionWidgets.erase(boxId);

	if(ptr)
	{
		ptr->deleteLater();
	}
    m_boxWidget->updateComboBox();
}


