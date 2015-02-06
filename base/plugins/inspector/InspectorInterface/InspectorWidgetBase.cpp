#include "InspectorSectionWidget.hpp"
#include "InspectorWidgetBase.hpp"

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

InspectorWidgetBase::InspectorWidgetBase (QObject* inspectedObj, QWidget* parent) :
	QWidget (parent), _inspectedObject {inspectedObj}
{
	QVBoxLayout* layout = new QVBoxLayout;
	layout->setMargin (5);
	setLayout (layout);

	// type
	_objectType = new QLabel ("type");

	// Name : label + lineEdit in a container
	QWidget* nameLine = new QWidget (this);
	QHBoxLayout* nameLayout = new QHBoxLayout;
	_objectName = new QLineEdit(this);
	nameLine->setObjectName ("ElementName");

	nameLayout->addWidget (_objectName);
	nameLayout->addStretch();
	nameLine->setLayout (nameLayout);

	// color
	_colorButton = new QPushButton(this);
	_colorButton->setMaximumSize (QSize (1.5 * m_colorIconSize, 1.5 * m_colorIconSize) );
	_colorButton->setIconSize (QSize (m_colorIconSize, m_colorIconSize) );
	setColor (Qt::gray);
	_colorButton->setIcon (QIcon (_colorButtonPixmap) );

	nameLayout->addWidget (_colorButton);
	nameLayout->addStretch(1);

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
	comments->setParent(this);
	comments->addContent (_comments);

	_sections.push_back (_objectType);
	_sections.push_back (nameLine);
	_sections.push_back (scrollArea);
	_sections.push_back (comments);

	updateSectionsView (layout, _sections);

	_scrollAreaLayout->addStretch();


	// Connection
	connect (_colorButton, SIGNAL (clicked() ), this, SLOT (changeColor() ) );

}


void InspectorWidgetBase::addNewSection (QString sectionName, QWidget* content)
{
	InspectorSectionWidget* section = new InspectorSectionWidget (sectionName, this);
	section->addContent (content);
	_scrollAreaLayout->addWidget (section);
}

void InspectorWidgetBase::addSubSection (QString parentSection, QString subSection, InspectorSectionWidget* content)
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

void InspectorWidgetBase::insertSection (int index, QString name, QWidget* content)
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

void InspectorWidgetBase::removeSection (QString sectionName)
{

}

void InspectorWidgetBase::updateSectionsView (QVBoxLayout* layout, std::vector<QWidget*>& contents)
{
	while (! layout->isEmpty() )
	{
		auto item = layout->takeAt(0);

		delete item->widget();
		delete item;
	}

	for (auto& section : contents)
	{
		layout->addWidget (section);
	}
}

void InspectorWidgetBase::changeColor()
{

	QColor color = QColorDialog::getColor (_currentColor, this, "Select Color");

	if (color.isValid() )
	{
		setColor (color);
	}
}

void InspectorWidgetBase::setName (QString newName)
{
	_objectName->setText (newName);
}

void InspectorWidgetBase::setComments (QString newComments)
{
	_comments->setText (newComments);
}

void InspectorWidgetBase::setColor (QColor newColor)
{
	_colorButtonPixmap.fill (newColor);
	_colorButton->setIcon (QIcon (_colorButtonPixmap) );
	_currentColor = newColor;
}

void InspectorWidgetBase::changeLabelType (QString type)
{
	_objectType->setText (type);
}

void InspectorWidgetBase::setInspectedObject (QObject* object)
{
	_inspectedObject = object;
}

