#include <core/view/View.hpp>
#include <interface/panel/PanelViewInterface.hpp>
#include <interface/panel/PanelPresenterInterface.hpp>
#include <interface/plugincontrol/MenuInterface.hpp>
#include <QDockWidget>
#include <QGridLayout>

#include <core/application/Application.hpp>
#include <core/document/DocumentView.hpp>

using namespace iscore;

View::View(QObject* parent):
	QMainWindow{}
{
	setObjectName("View");
}

void View::addPanel(PanelViewInterface* v)
{
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

	this->addDockWidget(v->defaultDock(), dial);
}

// @todo do this elsewhere... The View must not know the presenter.
void View::setPresenter(Presenter* pres)
{
	m_presenter = pres;

	auto view = m_presenter->document()->view();
	view->setParent(this);
	this->setCentralWidget(view);
}

