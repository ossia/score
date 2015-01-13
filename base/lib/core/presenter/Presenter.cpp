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

#include <QKeySequence>

using namespace iscore;

Presenter::Presenter(Model* model, View* view, QObject* arg_parent):
	NamedObject{"Presenter", arg_parent},
	m_model{model},
	m_view{view},
	#ifdef __APPLE__
	m_menubar{new QMenuBar, this},
	#else
	m_menubar{view->menuBar(), this},
	#endif
	m_document{new Document{view, this}}
{
	m_view->setCentralView(m_document->view());
	setupMenus();

	connect(m_view,		&View::insertActionIntoMenubar,
			&m_menubar, &MenubarManager::insertActionIntoMenubar);

	connect(m_document, &Document::on_elementSelected,
			this,		&Presenter::on_elementSelected);
}

void Presenter::setupCommand(PluginControlInterface* cmd)
{
	cmd->setParent(this); // Ownership transfer
	cmd->setPresenter(this);
	connect(cmd,  &PluginControlInterface::submitCommand,
			this, &Presenter::applyCommand, Qt::QueuedConnection);

	cmd->populateMenus(&m_menubar);
	cmd->populateToolbars();

	m_customCommands.push_back(cmd);
}

void Presenter::addPanel(PanelFactoryInterface* p)
{
	auto model = p->makeModel(m_model);
	auto view = p->makeView(m_view);
	auto pres = p->makePresenter(this, model, view);

	connect(pres, &PanelPresenterInterface::submitCommand,
			this, &Presenter::applyCommand, Qt::QueuedConnection);

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

void Presenter::applyCommand(iscore::SerializableCommand* cmd)
{
	m_document->presenter()->commandQueue()->pushAndEmit(cmd);
}

iscore::SerializableCommand* Presenter::instantiateUndoCommand(QString parent_name, QString name, QByteArray data)
{
	for(auto& ccmd : m_customCommands)
	{
		if(ccmd->objectName() == parent_name)
		{
			return ccmd->instantiateUndoCommand(name, data);
		}
	}

	qDebug() << "ALERT: Command" << parent_name << " :: " << name << "could not be instantiated.";
	return nullptr;
}

void Presenter::on_elementSelected(QObject* elt)
{
	emit elementSelected(elt);
}

#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
void Presenter::setupMenus()
{
	////// File //////
	auto newAct = m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
													   FileMenuElement::New,
													   std::bind(&Presenter::newDocument, this));

	newAct->setShortcut(QKeySequence::New);

	// ----------
	m_menubar.addSeparatorIntoToplevelMenu(ToplevelMenuElement::FileMenu,
										   FileMenuElement::Separator_Load);

	m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
										FileMenuElement::Load,
										[this] ()
	{
		auto loadname = QFileDialog::getOpenFileName(nullptr, tr("Open"));
		if(!loadname.isEmpty())
		{
			QFile f{loadname};
			if(f.open(QIODevice::ReadOnly))
			{
				m_document->load(f.readAll());
			}
		}
	});

	// Load & save
	m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
										FileMenuElement::Save,
										[this] ()
	{
		auto savename = QFileDialog::getSaveFileName(nullptr, tr("Save"));

		if(!savename.isEmpty())
		{
			QFile f(savename);
			f.open(QIODevice::WriteOnly);
			f.write(m_document->save());
		}
	});

//	m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
//										FileMenuElement::SaveAs,
//										notyet);

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
	undoAct->setShortcut(QKeySequence::Undo);
	connect(undoAct,								 &QAction::triggered,
			m_document->presenter()->commandQueue(), &CommandQueue::onUndo);
	m_menubar.insertActionIntoToplevelMenu(ToplevelMenuElement::EditMenu,
										   undoAct);

	auto redoAct = m_document->presenter()->commandQueue()->createRedoAction(this);
	redoAct->setShortcut(QKeySequence::Redo);
	connect(redoAct,								 &QAction::triggered,
			m_document->presenter()->commandQueue(), &CommandQueue::onRedo);
	m_menubar.insertActionIntoToplevelMenu(ToplevelMenuElement::EditMenu,
										   redoAct);

	////// View //////
	m_menubar.addMenuIntoToplevelMenu(ToplevelMenuElement::ViewMenu,
									  ViewMenuElement::Windows);

	////// Settings //////
	m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::SettingsMenu,
										SettingsMenuElement::Settings,
										std::bind(&SettingsView::exec,
												  qobject_cast<Application*>(parent())->settings()->view()));
}


void Presenter::on_lock(QByteArray arr)
{
	ObjectPath objectToLock;

	Deserializer<DataStream> s{&arr};
	s.writeTo(objectToLock);

	auto obj = objectToLock.find<QObject>();
	QMetaObject::invokeMethod(obj, "lock");
}

void Presenter::on_unlock(QByteArray arr)
{
	ObjectPath objectToUnlock;

	Deserializer<DataStream> s{&arr};
	s.writeTo(objectToUnlock);

	auto obj = objectToUnlock.find<QObject>();
	QMetaObject::invokeMethod(obj, "unlock");
}
