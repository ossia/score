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
/*
    m_view->addSidePanel(new QUndoView{
                             m_document->presenter()->commandQueue(),
                             m_view}, tr("Undo View"), Qt::LeftDockWidgetArea);
*/
}

void Presenter::registerPluginControl(PluginControlInterface* cmd)
{
    cmd->setParent(this);  // Ownership transfer
    cmd->setPresenter(this);

    // TODO in DocumentPresenter
    //connect(cmd,  &PluginControlInterface::submitCommand,
    //        this, &Presenter::applyCommand, Qt::QueuedConnection);

    cmd->populateMenus(&m_menubar);
    cmd->populateToolbars();

    m_customControls.push_back(cmd);
}

void Presenter::registerPanel(PanelFactoryInterface* factory)
{
    auto view = factory->makeView(m_view);
    auto pres = factory->makePresenter(this, view);

    m_panelPresenters.push_back({pres, factory});

    // TODO in DocumentPresenter
    //connect(pres, &PanelPresenterInterface::submitCommand,
    //        this, &Presenter::applyCommand, Qt::QueuedConnection);

    m_view->setupPanelView(view);

    for(auto doc : m_documents)
        doc->setupNewPanel(pres, factory);
}

void Presenter::registerDocumentPanel(DocumentDelegateFactoryInterface* docpanel)
{
    m_availableDocuments.push_back(docpanel);
}

void Presenter::newDocument(DocumentDelegateFactoryInterface* doctype)
{
    auto doc = new Document{doctype, m_view, this};
    m_documents.push_back(doc);
    m_currentDocument = doc;

    m_view->setCentralView(m_currentDocument->view());

    for(auto& panel : m_panelPresenters)
    {
        doc->setupNewPanel(panel.first, panel.second);
    }
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
*/
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
