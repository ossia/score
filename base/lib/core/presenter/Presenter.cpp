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

#include <core/undo/UndoControl.hpp>
#include <core/undo/UndoView.hpp>
#include <functional>

#include <QKeySequence>
#include <QFileDialog>

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

    registerPluginControl(new UndoControl{this});
    registerPanel(new UndoPanelFactory);
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

Document *Presenter::currentDocument() const
{
    return m_currentDocument;
}

void Presenter::setCurrentDocument(Document* doc)
{
    m_currentDocument = doc;
    for(auto& pair : m_panelPresenters)
    {
        m_currentDocument->bindPanelPresenter(pair.first);
    }

    m_view->setCentralView(m_currentDocument->view());
    emit currentDocumentChanged(m_currentDocument);
}

void Presenter::newDocument(DocumentDelegateFactoryInterface* doctype)
{
    addDocument(new Document{doctype, m_view, this});
}

void Presenter::loadDocument(const QByteArray& data,
                             DocumentDelegateFactoryInterface* doctype)
{
    addDocument(new Document{data, doctype, m_view, this});
}

void Presenter::addDocument(Document* doc)
{
    m_documents.push_back(doc);

    for(auto& panel : m_panelPresenters)
    {
        doc->setupNewPanel(panel.first, panel.second);
    }

    setCurrentDocument(doc);
}

// TODO make a class whose purpose is to instantiate commands.
iscore::SerializableCommand* Presenter::instantiateUndoCommand(const QString& parent_name,
                                                               const QString& name,
                                                               const QByteArray& data)
{
    for(auto& ccmd : m_customControls)
    {
        if(ccmd->objectName() == parent_name)
        {
            return ccmd->instantiateUndoCommand(name, data);
        }
    }

    qDebug() << "ALERT: Command" << parent_name << "::" << name << "could not be instantiated.";
    return nullptr;
}

void Presenter::setupMenus()
{
    ////// File //////
    auto newAct = m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                                      FileMenuElement::New,
                                 [&] () { newDocument(m_availableDocuments.front()); });

    newAct->setShortcut(QKeySequence::New);

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
                loadDocument(f.readAll(), m_availableDocuments.front());
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
            f.write(currentDocument()->save());
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

    ////// View //////
    m_menubar.addMenuIntoToplevelMenu(ToplevelMenuElement::ViewMenu,
                                      ViewMenuElement::Windows);

    ////// Settings //////
    m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::SettingsMenu,
                                        SettingsMenuElement::Settings,
                                        std::bind(&SettingsView::exec,
                                                  qobject_cast<Application*> (parent())->settings()->view()));
}


// TODO this goes somewhere elses
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

