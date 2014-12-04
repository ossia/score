#include "InspectorSectionWidget.hpp"
#include "InspectorWidgetInterface.hpp"

#include <QLayout>
#include <QLineEdit>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QSpacerItem>
#include <QScrollArea>
#include <QScrollBar>
#include <QDebug>
#include <QColorDialog>

static const int COLOR_ICON_SIZE = 21;

InspectorWidgetInterface::InspectorWidgetInterface (QObject* inspectedObj, QWidget* parent) :
	QWidget (parent), _inspectedObject {inspectedObj}
{
	_sections = new std::vector<QWidget*>;

	QVBoxLayout* layout = new QVBoxLayout;
	layout->setMargin (5);
	setLayout (layout);

	// type
	_objectType = new QLabel ("type");

	// Name : label + lineEdit in a container
	QWidget* nameLine = new QWidget (this);
	QHBoxLayout* nameLayout = new QHBoxLayout;
	_objectName = new QLineEdit;
	nameLine->setObjectName ("ElementName");

	nameLayout->addWidget (_objectName);
	nameLayout->addStretch();
	nameLine->setLayout (nameLayout);

	// color
	_colorButton = new QPushButton;
	_colorButton->setMaximumSize (QSize (1.5 * COLOR_ICON_SIZE, 1.5 * COLOR_ICON_SIZE) );
	_colorButton->setIconSize (QSize (COLOR_ICON_SIZE, COLOR_ICON_SIZE) );
	_colorButtonPixmap = new QPixmap (4 * COLOR_ICON_SIZE / 3, 4 * COLOR_ICON_SIZE / 3);
	setColor (Qt::gray);
	_colorButton->setIcon (QIcon (*_colorButtonPixmap) );

	nameLayout->addWidget (_colorButton);
	nameLayout->addStretch();

	// scroll Area
	_scrollAreaLayout = new QVBoxLayout;
	QScrollArea* scrollArea = new QScrollArea;
	QWidget* scrollAreaContentWidget = new QWidget;
	scrollArea->setWidgetResizable (true);

	scrollAreaContentWidget->setLayout (_scrollAreaLayout);
	scrollArea->setWidget (scrollAreaContentWidget);

	// comments
	_comments = new QTextEdit;
	InspectorSectionWidget* comments = new InspectorSectionWidget ("Comments");
	comments->addContent (_comments);

	_sections->push_back (_objectType);
	_sections->push_back (nameLine);
	_sections->push_back (scrollArea);
	_sections->push_back (comments);

	updateSectionsView (layout, _sections);

	_scrollAreaLayout->addStretch();


	// Connection
	connect (_colorButton, SIGNAL (clicked() ), this, SLOT (changeColor() ) );

}


void InspectorWidgetInterface::addNewSection (QString sectionName, QWidget* content)
{
	InspectorSectionWidget* section = new InspectorSectionWidget (sectionName, this);
	section->addContent (content);
	_scrollAreaLayout->addWidget (section);
}

void InspectorWidgetInterface::addSubSection (QString parentSection, QString subSection, InspectorSectionWidget* content)
{
	InspectorSectionWidget* section = findChild<InspectorSectionWidget*> (parentSection);

	if (section != nullptr)
	{
		{
			content->renameSection (subSection);
			content->setObjectName (subSection);
			section->addContent (content);
		}
	}
}

void InspectorWidgetInterface::insertSection (int index, QString name, QWidget* content)
{
	if (index < 0)
	{
		index += _scrollAreaLayout->count();
	}

	InspectorSectionWidget* section = new InspectorSectionWidget (this);
	section->renameSection (name);
	section->setObjectName (name);

	if (content)
	{
		section->addContent (content);
	}

	_scrollAreaLayout->insertWidget (index, section);
}

void InspectorWidgetInterface::removeSection (QString sectionName)
{

}

void InspectorWidgetInterface::updateSectionsView (QVBoxLayout* layout, std::vector<QWidget*>* contents)
{
	while (! layout->isEmpty() )
	{
		delete layout->itemAt (0)->widget();
	}

	for (auto& section : *contents)
	{
		layout->addWidget (section);
	}
}

void InspectorWidgetInterface::changeColor()
{

	QColor color = QColorDialog::getColor (_currentColor, this, "Select Color");

	if (color.isValid() )
	{
		setColor (color);
	}
}

void InspectorWidgetInterface::setName (QString newName)
{
	_objectName->setText (newName);
}

void InspectorWidgetInterface::setComments (QString newComments)
{
	_comments->setText (newComments);
}

void InspectorWidgetInterface::setColor (QColor newColor)
{
	_colorButtonPixmap->fill (newColor);
	_colorButton->setIcon (QIcon (*_colorButtonPixmap) );
	_currentColor = newColor;
}

void InspectorWidgetInterface::changeLabelType (QString type)
{
	_objectType->setText (type);
}

void InspectorWidgetInterface::setInspectedObject (QObject* object)
{
	_inspectedObject = object;
}

