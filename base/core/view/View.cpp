#include <core/view/View.hpp>
#include <interface/panels/Panel.hpp>
#include <interface/customcommand/MenuInterface.hpp>
#include <QDockWidget>
#include <QGridLayout>

#include <core/application/Application.hpp>
#include <core/document/DocumentView.hpp>

using namespace iscore;

View::View(QObject* parent):
	QMainWindow{}
{
}

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

	emit insertActionIntoMenubar({MenuInterface::name(ToplevelMenuElement::ViewMenu) + "/" +
								  MenuInterface::name(ViewMenuElement::Windows),
								  hideDialog});

	this->addDockWidget(Qt::RightDockWidgetArea, dial);
}

// TODO devenir updateDocument ?
void View::setCentralPanel(PanelView* v)
{/*
	v->setParent(this);
	m_panelsViews.insert(v);

	this->setCentralWidget(v->getWidget());*/
}

void View::setPresenter(Presenter* pres)
{
	m_presenter = pres;

	auto view = m_presenter->document()->view();
	view->setParent(this);
	this->setCentralWidget(view);
}

