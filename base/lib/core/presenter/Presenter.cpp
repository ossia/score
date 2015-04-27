#include <iscore/plugins/plugincontrol/PluginControlInterface.hpp>

#include <core/application/Application.hpp>
#include <core/view/View.hpp>

#include <iscore/plugins/panel/PanelFactoryInterface.hpp>
#include <iscore/plugins/panel/PanelPresenterInterface.hpp>
#include <iscore/plugins/panel/PanelModelInterface.hpp>
#include <iscore/plugins/panel/PanelViewInterface.hpp>

#include <core/document/DocumentPresenter.hpp>
#include <core/document/DocumentView.hpp>

#include <core/undo/UndoControl.hpp>

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

    connect(m_view, &View::closeRequested,
            this,   &Presenter::closeDocument);
}

#include <QToolBar>
void Presenter::registerPluginControl(PluginControlInterface* cmd)
{
    cmd->setParent(this);  // Ownership transfer
    cmd->setPresenter(this);

    cmd->populateMenus(&m_menubar);
    auto bars = cmd->makeToolbars();
    for(auto& bar : bars)
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

    for(auto& ctrl : m_customControls)
    {
        ctrl->on_documentChanged(m_currentDocument);
    }
}

#include <QMessageBox>
void Presenter::closeDocument(Document* doc)
{
    // Warn the user if he might loose data
    if(!doc->commandStack().isAtSavedIndex())
    {
        QMessageBox msgBox;
        msgBox.setText(tr("The document has been modified."));
        msgBox.setInformativeText(tr("Do you want to save your changes?"));
        msgBox.setStandardButtons(QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        msgBox.setDefaultButton(QMessageBox::Save);
        int ret = msgBox.exec();
        switch (ret)
        {
        case QMessageBox::Save:
            saveJson(doc);
            break;
        case QMessageBox::Discard:
            // Do nothing
            break;
        case QMessageBox::Cancel:
            return;
            break;
        default:
            break;
        }
    }

    // Close operation
    m_view->closeDocument(doc->view());
    m_documents.removeOne(doc);

    if(m_documents.size() > 0)
    {
        setCurrentDocument(m_documents.last());
    }
    else
    {
        for(auto& pair : m_panelPresenters)
        {
            pair.first->setModel(nullptr);
        }

        for(auto& ctrl : m_customControls)
        {
            ctrl->on_documentChanged(nullptr);
        }
    }

    delete doc;
}

void Presenter::saveBinary(Document * doc)
{
    QFileDialog d{nullptr, tr("Save (Binary)")};
    d.setNameFilter("*.scorebin");
    d.setDefaultSuffix("scorebin");
    d.setConfirmOverwrite(true);
    d.setFileMode(QFileDialog::AnyFile);
    d.setAcceptMode(QFileDialog::AcceptSave);

    if(d.exec())
    {
        auto savename = d.selectedFiles().first();
        if(!savename.isEmpty())
        {
            QFile f{savename};
            f.open(QIODevice::WriteOnly);
            f.write(doc->saveAsByteArray());
        }
    }
}

void Presenter::saveJson(Document * doc)
{
    QFileDialog d{nullptr, tr("Save (JSON)")};
    d.setDefaultSuffix("scorejson");
    d.setNameFilter("*.scorejson");
    d.setConfirmOverwrite(true);
    d.setFileMode(QFileDialog::AnyFile);
    d.setAcceptMode(QFileDialog::AcceptSave);

    if(d.exec())
    {
        auto savename = d.selectedFiles().first();
        if(!savename.isEmpty())
        {
            QFile f(savename);
            f.open(QIODevice::WriteOnly);

            QJsonDocument json_doc;
            json_doc.setObject(doc->saveAsJson());

            f.write(json_doc.toJson());
        }
    }
}

void Presenter::loadBinary()
{
    auto loadname = QFileDialog::getOpenFileName(nullptr, tr("Load (Binary)"), QString(), "*.scorebin");

    if(!loadname.isEmpty())
    {
        QFile f {loadname};
        if(f.open(QIODevice::ReadOnly))
        {
            loadDocument(f.readAll(), m_availableDocuments.front());
        }
    }
}

void Presenter::loadJson()
{
    auto loadname = QFileDialog::getOpenFileName(nullptr, tr("Load (JSON)"), QString(), "*.scorejson");

    if(!loadname.isEmpty())
    {
        QFile f(loadname);
        if(f.open(QIODevice::ReadOnly))
        {
            auto doc = QJsonDocument::fromJson(f.readAll());
            loadDocument(doc.object(), m_availableDocuments.front());
        }
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

    //// Save and load
    // Binary
    m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                        FileMenuElement::Load,
                                        [this]() { loadBinary(); });

    m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                        FileMenuElement::Save,
                                        [this]() { saveBinary(currentDocument()); });

    // Json
    auto fromJson = new QAction(tr("Load (JSON)"), this);
    connect(fromJson, &QAction::triggered, this, &Presenter::loadJson);
    m_menubar.insertActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                       FileMenuElement::Separator_Quit,
                                       fromJson);


    auto toJson = new QAction(tr("Save (JSON)"), this);
    connect(toJson, &QAction::triggered,
            [this]() { saveJson(currentDocument()); });


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
