#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>
#include <iscore/command/Command.hpp>

#include <core/application/Application.hpp>
#include <core/view/View.hpp>

#include <iscore/plugins/panel/PanelFactoryInterface.hpp>
#include <iscore/plugins/panel/PanelPresenterInterface.hpp>
#include <iscore/plugins/panel/PanelModelInterface.hpp>
#include <iscore/plugins/panel/PanelViewInterface.hpp>

#include <core/document/DocumentPresenter.hpp>
#include <core/document/DocumentView.hpp>

#include <core/undo/UndoControl.hpp>
#include <core/undo/UndoView.hpp>
#include <functional>

#include <QKeySequence>
#include <QFileDialog>
#include <QJsonDocument>

using namespace iscore;

Presenter::Presenter(View* view, QObject* arg_parent) :
    NamedObject {"Presenter", arg_parent},
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
    connect(m_view, &View::activeDocumentChanged,
            this,   &Presenter::setCurrentDocument);

    registerPluginControl(new UndoControl{this});
    registerPanel(new UndoPanelFactory);
}

#include <QToolBar>
void Presenter::registerPluginControl(PluginControlInterface* cmd)
{
    cmd->setParent(this);  // Ownership transfer
    cmd->setPresenter(this);

    cmd->populateMenus(&m_menubar);
    QToolBar* bar = new QToolBar;
    cmd->populateToolbars(bar);
    m_view->addToolBar(bar);
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

    for(PluginControlInterface* ctrl : m_customControls)
    {
        ctrl->on_documentChanged(m_currentDocument);
    }
}

void Presenter::newDocument(DocumentDelegateFactoryInterface* doctype)
{
    auto doc = new Document{doctype, m_view, this};

    m_documents.push_back(doc);

    for(auto& panel : m_panelPresenters)
    {
        doc->setupNewPanel(panel.first, panel.second);
    }

    m_view->addDocumentView(doc->view());

    setCurrentDocument(doc);
}

Document* Presenter::loadDocument(const QVariant& data,
                                  DocumentDelegateFactoryInterface* doctype)
{
    auto doc = new Document{data, doctype, m_view, this};

    m_documents.push_back(doc);
    m_view->addDocumentView(doc->view());

    setCurrentDocument(doc);

    return doc;
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


    m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                        FileMenuElement::Save,
                                        [this]()
    {
        auto savename = QFileDialog::getSaveFileName(nullptr, tr("Save (Binary)"));

        if(!savename.isEmpty())
        {
            QFile f(savename);
            f.open(QIODevice::WriteOnly);
            f.write(currentDocument()->saveAsByteArray());
        }
    });


    auto fromJson = new QAction(tr("Load (JSON)"), this);
    connect(fromJson, &QAction::triggered,
            [this]()
    {
        auto loadname = QFileDialog::getOpenFileName(nullptr, tr("Save (JSON)"));

        if(!loadname.isEmpty())
        {
            QFile f(loadname);
            if(f.open(QIODevice::ReadOnly))
            {
                auto doc = QJsonDocument::fromJson(f.readAll());
                loadDocument(doc.object(), m_availableDocuments.front());
            }
        }
    });

    m_menubar.insertActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                       FileMenuElement::Separator_Quit,
                                       fromJson);

    auto toJson = new QAction(tr("Save (JSON)"), this);
    connect(toJson, &QAction::triggered,
            [this]()
    {
        auto savename = QFileDialog::getSaveFileName(nullptr, tr("Save (JSON)"));

        if(!savename.isEmpty())
        {
            QFile f(savename);
            f.open(QIODevice::WriteOnly);

            QJsonDocument doc;
            doc.setObject(currentDocument()->saveAsJson());

            f.write(doc.toJson());
        }
    });

    m_menubar.insertActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                       FileMenuElement::Separator_Quit,
                                       toJson);

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
