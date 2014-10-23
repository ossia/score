#include <core/view/View.hpp>
#include <interface/panels/Panel.hpp>
#include <interface/customcommand/MenuInterface.hpp>
#include <QDockWidget>
#include <QGridLayout>

#include <core/application/Application.hpp>
using namespace iscore;

View::View(QObject* parent):
	QMainWindow{},
	m_application{qobject_cast<Application*>(parent)}
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

void View::setCentralPanel(PanelView* v)
{
	v->setParent(this);
	m_panelsViews.insert(v);

	this->setCentralWidget(v->getWidget());
}


//#include <API/Headers/Repartition/session/ClientSessionBuilder.h>
#include <core/view/dialogs/zeroconf/ZeroConfConnectDialog.hpp>
void View::createZeroconfSelectionDialog()
{
	auto diag = new ZeroconfConnectDialog(this);

	connect(diag,			&ZeroconfConnectDialog::connectedTo,
			m_application,	&Application::setupClientSession);

	diag->exec();
}
