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

#include <core/panels/UndoView.hpp>
#include <functional>

#include <QKeySequence>

#include <QFileDialog>
#include <QUndoView>

using namespace iscore;

Presenter::Presenter(Model* model, View* view, QObject* arg_parent) :
    NamedObject {"Presenter", arg_parent},
    m_model {model},
    m_view {view},
    #ifdef __APPLE__
    m_menubar {new QMenuBar, this}
  #else
    m_menubar {view->menuBar(), this}
  #endif
{
    setupMenus();
    connect(m_view,		&View::insertActionIntoMenubar,
            &m_menubar, &MenubarManager::insertActionIntoMenubar);

    m_undoAction->setShortcut(QKeySequence::Undo);
    m_undoAction->setEnabled(false);
    m_undoAction->setText(tr("Undo"));
    connect(m_undoAction, &QAction::triggered,
            [&] ()
    {
        m_currentDocument->presenter()->commandQueue()->undoAndNotify();
    });

    m_redoAction->setShortcut(QKeySequence::Redo);
    m_redoAction->setEnabled(false);
    m_redoAction->setText(tr("Redo"));
    connect(m_redoAction, &QAction::triggered,
            [&] ()
    {
        m_currentDocument->presenter()->commandQueue()->redoAndNotify();
    });

    registerPanel(new UndoPanelFactory);
}

Presenter::~Presenter()
{
    for(auto& conn : m_connections)
        disconnect(conn);
}

void Presenter::registerPluginControl(PluginControlInterface* cmd)
{
    cmd->setParent(this);  // Ownership transfer
    cmd->setPresenter(this);

    cmd->populateMenus(&m_menubar);
    cmd->populateToolbars();

    m_customControls.push_back(cmd);
}

void Presenter::registerPanel(PanelFactoryInterface* factory)
{
    auto view = factory->makeView(m_view);
    auto pres = factory->makePresenter(this, view);

    m_panelPresenters.push_back({pres, factory});

    m_view->setupPanelView(view);

    for(auto doc : m_documents)
        doc->setupNewPanel(pres, factory);
}

void Presenter::registerDocumentPanel(DocumentDelegateFactoryInterface* docpanel)
{
    m_availableDocuments.push_back(docpanel);
}

void Presenter::setCurrentDocument(Document* doc)
{
    // Disconnect previously registered connections.
    for(auto& conn : m_connections)
        disconnect(conn);

    m_currentDocument = doc;
    for(auto& pair : m_panelPresenters)
    {
        m_currentDocument->bindPanelPresenter(pair.first);
    }

    m_view->setCentralView(m_currentDocument->view());

    // TODO put this in the UndoView, and put the CommandQueue inside.
    m_connections.push_back(
                connect(m_currentDocument->presenter()->commandQueue(), &CommandStack::canUndoChanged,
                        [&] (bool b) { m_undoAction->setEnabled(b); }));
    m_connections.push_back(
                connect(m_currentDocument->presenter()->commandQueue(), &CommandStack::canRedoChanged,
                        [&] (bool b) { m_redoAction->setEnabled(b); }));

    m_connections.push_back(
                connect(m_currentDocument->presenter()->commandQueue(), &CommandStack::undoTextChanged,
                        [&] (const QString& s) { m_undoAction->setText(tr("Undo ") + s);}));
    m_connections.push_back(
                connect(m_currentDocument->presenter()->commandQueue(), &CommandStack::redoTextChanged,
                        [&] (const QString& s) { m_redoAction->setText(tr("Redo ") + s);}));
}

void Presenter::newDocument(DocumentDelegateFactoryInterface* doctype)
{
    auto doc = new Document{doctype, m_view, this};
    m_documents.push_back(doc);

    for(auto& panel : m_panelPresenters)
    {
        doc->setupNewPanel(panel.first, panel.second);
    }

    setCurrentDocument(doc);
}

iscore::SerializableCommand* Presenter::instantiateUndoCommand(const QString& parent_name, const QString& name, const QByteArray& data)
{
    for(auto& ccmd : m_customControls)
    {
        if(ccmd->objectName() == parent_name)
        {
            return ccmd->instantiateUndoCommand(name, data);
        }
    }

    qDebug() << "ALERT: Command" << parent_name << " :: " << name << "could not be instantiated.";
    return nullptr;
}

void Presenter::setupMenus()
{
    ////// File //////
    auto newAct = m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                                      FileMenuElement::New,
                                 [&] () { newDocument(m_availableDocuments.front()); });


    newAct->setShortcut(QKeySequence::New);
/*
    // ----------
    m_menubar.addSeparatorIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                           FileMenuElement::Separator_Load);

    m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                        FileMenuElement::Load,
                                        [this]()
    {
        auto loadname = QFileDialog::getOpenFileName(nullptr, tr("Open"));

        if(!loadname.isEmpty())
        {
            QFile f {loadname};

            if(f.open(QIODevice::ReadOnly))
            {
                m_document->load(f.readAll());
            }
        }
    });



    // Load & save
    m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                        FileMenuElement::Save,
                                        [this]()
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
*/
    ////// Edit //////
    m_menubar.insertActionIntoToplevelMenu(ToplevelMenuElement::EditMenu,
                                        EditMenuElement::Undo,
                                        m_undoAction);
    m_menubar.insertActionIntoToplevelMenu(ToplevelMenuElement::EditMenu,
                                        EditMenuElement::Redo,
                                        m_redoAction);

    ////// View //////
    m_menubar.addMenuIntoToplevelMenu(ToplevelMenuElement::ViewMenu,
                                      ViewMenuElement::Windows);

    ////// Settings //////
    m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::SettingsMenu,
                                        SettingsMenuElement::Settings,
                                        std::bind(&SettingsView::exec,
                                                  qobject_cast<Application*> (parent())->settings()->view()));
}


void Presenter::on_lock(QByteArray arr)
{
    ObjectPath objectToLock;

    Deserializer<DataStream> s {&arr};
    s.writeTo(objectToLock);

    auto obj = objectToLock.find<QObject>();
    QMetaObject::invokeMethod(obj, "lock");
}

void Presenter::on_unlock(QByteArray arr)
{
    ObjectPath objectToUnlock;

    Deserializer<DataStream> s {&arr};
    s.writeTo(objectToUnlock);

    auto obj = objectToUnlock.find<QObject>();
    QMetaObject::invokeMethod(obj, "unlock");
}
