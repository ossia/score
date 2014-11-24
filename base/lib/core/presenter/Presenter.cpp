#include <interface/plugincontrol/PluginControlInterface.hpp>
#include <core/presenter/command/Command.hpp>

#include <core/application/Application.hpp>
#include <core/view/View.hpp>
#include <core/model/Model.hpp>

#include <interface/panel/PanelFactoryInterface.hpp>
#include <interface/panel/PanelPresenterInterface.hpp>
#include <interface/panel/PanelModelInterface.hpp>
#include <interface/panel/PanelViewInterface.hpp>

#include <core/document/DocumentPresenter.hpp>
#include <core/document/DocumentView.hpp>

#include <functional>

using namespace iscore;

Presenter::Presenter(Model* model, View* view, QObject* arg_parent):
	QObject{arg_parent},
	m_model{model},
	m_view{view},
	m_menubar{view->menuBar(), this},
	m_document{new Document{this, view}}
{
	setObjectName("Presenter");
	m_view->setPresenter(this);
	setupMenus();

	connect(m_view,		&View::insertActionIntoMenubar,
			&m_menubar, &MenubarManager::insertActionIntoMenubar);

}

// TODO is this the place ?
void Presenter::setupCommand(PluginControlInterface* cmd)
{
	cmd->setParent(this); // Ownership transfer
	cmd->setPresenter(this);
	connect(cmd,  &PluginControlInterface::submitCommand,
			this, &Presenter::applyCommand);

	cmd->populateMenus(&m_menubar);
	cmd->populateToolbars();

	m_customCommands.push_back(cmd);
}

// TODO is this the place ?
void Presenter::addPanel(PanelFactoryInterface* p)
{
	auto model = p->makeModel();
	auto view = p->makeView();
	auto pres = p->makePresenter(this, model, view);

	connect(pres, &PanelPresenterInterface::submitCommand,
			this, &Presenter::applyCommand);

	view->setPresenter(pres);
	model->setPresenter(pres);

	m_view->addPanel(view);
	m_model->addPanel(model);
	m_panelsPresenters.insert(pres);
}

void Presenter::setDocumentPanel(DocumentDelegateFactoryInterface* docpanel)
{
	m_document->setDocumentPanel(docpanel);
}

void Presenter::newDocument()
{
	m_document->newDocument();
}

void Presenter::applyCommand(SerializableCommand* cmd)
{
	m_document->presenter()->commandQueue()->pushAndEmit(cmd);
}

void Presenter::instantiateUndoCommand(QString parent_name, QString name, QByteArray data)
{
	for(auto& ccmd : m_customCommands)
	{
		if(ccmd->objectName() == parent_name)
		{
			emit instantiatedCommand(ccmd->instantiateUndoCommand(name, data));
		}
	}
}

#include <QMessageBox>
void Presenter::setupMenus()
{
	//TODO
	auto notyet = [] { QMessageBox::information(nullptr, "Not yet", "Not yet");};

	////// File //////
	m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
										FileMenuElement::New,
										std::bind(&Presenter::newDocument, this));

	// ----------
	m_menubar.addSeparatorIntoToplevelMenu(ToplevelMenuElement::FileMenu,
										   FileMenuElement::Separator_Load);

	m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
										FileMenuElement::Load,
										notyet);
	m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
										FileMenuElement::Save,
										notyet);
	m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
										FileMenuElement::SaveAs,
										notyet);

	// ----------
	m_menubar.addSeparatorIntoToplevelMenu(ToplevelMenuElement::FileMenu,
										   FileMenuElement::Separator_Export);

	// ----------
	m_menubar.addSeparatorIntoToplevelMenu(ToplevelMenuElement::FileMenu,
										   FileMenuElement::Separator_Quit);

	m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
										FileMenuElement::Quit,
										&QApplication::quit);

	////// Edit //////
	// Undo / redo
	auto undoAct = m_document->presenter()->commandQueue()->createUndoAction(this);
	connect(undoAct,								 &QAction::triggered,
			m_document->presenter()->commandQueue(), &CommandQueue::onUndo);
	m_menubar.insertActionIntoToplevelMenu(ToplevelMenuElement::EditMenu,
										   undoAct);

	auto redoAct = m_document->presenter()->commandQueue()->createRedoAction(this);
	connect(redoAct,								 &QAction::triggered,
			m_document->presenter()->commandQueue(), &CommandQueue::onRedo);
	m_menubar.insertActionIntoToplevelMenu(ToplevelMenuElement::EditMenu,
										   redoAct);

	////// Settings //////
	m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::SettingsMenu,
										SettingsMenuElement::Settings,
										std::bind(&SettingsView::exec,
												  qobject_cast<Application*>(parent())->settings()->view()));
}


