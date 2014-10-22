#include <core/view/View.hpp>
#include <interface/panels/Panel.hpp>
#include <interface/customcommand/MenuInterface.hpp>
#include <QDockWidget>
#include <QGridLayout>

using namespace iscore;

void View::addPanel(PanelView* v)
{
	v->setParent(this);
	m_panelsViews.insert(v);
	
	QDockWidget* dial = new QDockWidget(this);
	dial->setWidget(v->getWidget());
	
	QAction* hideDialog = new QAction(v->objectName(), nullptr);
	hideDialog->setCheckable(true);
	hideDialog->setChecked(true);
	
	connect(hideDialog, &QAction::triggered,
			dial,		&QDockWidget::setVisible);
	
	MenuInterface m;
	emit insertActionIntoMenubar({m.name(ToplevelMenuElement::ViewMenu) + "/" + 
								  m.name(ViewMenuElement::Windows), 
								  hideDialog});
	
	this->addDockWidget(Qt::RightDockWidgetArea, dial);
}

void View::setCentralPanel(PanelView* v)
{
	v->setParent(this);
	m_panelsViews.insert(v);
	
	this->setCentralWidget(v->getWidget());
}
