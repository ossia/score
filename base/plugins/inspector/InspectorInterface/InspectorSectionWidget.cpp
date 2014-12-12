#include "InspectorSectionWidget.hpp"

#include <QLayout>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>
#include <QPushButton>
#include <QDebug>
#include <QScrollArea>
#include <QScrollBar>

InspectorSectionWidget::InspectorSectionWidget (QWidget* parent) :
	QWidget (parent)
{
	// HEADER : arrow button and name
	QWidget* title = new QWidget;
	QHBoxLayout* titleLayout = new QHBoxLayout;
	_sectionTitle = new QLineEdit; //QLabel ?;
	_sectionTitle->setStyleSheet ( QString ( "background-color: lightGray;") );
	_btn = new QToolButton;

	_buttonTitle = new QPushButton;
	_buttonTitle->setFlat (true);
	_buttonTitle->setText ("section name");
	_buttonTitle->setStyleSheet ("text-align: left;");
	_buttonTitle->setLayout (new QVBoxLayout);
	_buttonTitle->layout()->addWidget (_sectionTitle);
	_buttonTitle->layout()->setMargin (0);
	_sectionTitle->hide();

	titleLayout->addWidget (_btn);
	titleLayout->addWidget (_buttonTitle);
	title->setLayout (titleLayout);

	// CONTENT
	/*    _container = new QScrollArea;
	    QWidget *areaContent = new QWidget;

	    _container->setMinimumSize(150,50);
	    _container->setWidgetResizable(true);
	    _containerLayout = new QVBoxLayout;
	    _containerLayout->setMargin(0);
	    _containerLayout->addStretch();
	    areaContent->setLayout(_containerLayout);
	    _container->setWidget(areaContent);
	*/
	_container = new QWidget;
	_containerLayout = new QVBoxLayout;
	_containerLayout->setMargin (1);
	_containerLayout->addStretch();
	_container->setLayout (_containerLayout);

	// GENERAL
	QVBoxLayout* globalLayout = new QVBoxLayout;
	globalLayout->addWidget (title);
	globalLayout->addWidget (_container);
	globalLayout->setMargin (5);

	connect (_btn, SIGNAL (released() ), this, SLOT (expend() ) );
	connect (_buttonTitle, SIGNAL (clicked() ), this, SLOT (nameEditEnable() ) );
	connect (_sectionTitle, SIGNAL (editingFinished() ), this, SLOT (nameEditDisable() ) );


	// INIT
	_isUnfolded = true;
	_btn->setArrowType (Qt::DownArrow);
//   expend();

	setLayout (globalLayout);

	renameSection ("Section Name");
}

InspectorSectionWidget::InspectorSectionWidget (QString name, QWidget* parent) :
	InspectorSectionWidget (parent)
{
	renameSection (name);
	setObjectName (name);
}

InspectorSectionWidget::~InspectorSectionWidget()
{

}

void InspectorSectionWidget::expend()
{
	_isUnfolded = !_isUnfolded;
	_container->setVisible (_isUnfolded);

	if (_isUnfolded)
	{
		_btn->setArrowType (Qt::DownArrow);
	}
	else
	{
		_btn->setArrowType (Qt::RightArrow);
	}
}

void InspectorSectionWidget::renameSection (QString newName)
{
	_sectionTitle->setText (newName);
	_buttonTitle->setText (newName);
}

void InspectorSectionWidget::addContent (QWidget* newWidget)
{
	_containerLayout->insertWidget (_containerLayout->count() - 1, newWidget);
}

void InspectorSectionWidget::insertInSection (int index, QWidget* newWidget)
{
	_containerLayout->insertWidget (index, newWidget);
	_container->setLayout (_containerLayout);
}

void InspectorSectionWidget::nameEditEnable()
{
	_sectionTitle->show();
	_sectionTitle->setFocus();
}

void InspectorSectionWidget::nameEditDisable()
{
	_sectionTitle->hide();
	_buttonTitle->setText (_sectionTitle->text() );
}
