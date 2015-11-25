#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>

#include <core/application/Application.hpp>
#include <core/view/View.hpp>

#include <iscore/plugins/panel/PanelView.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <core/document/DocumentPresenter.hpp>
#include <core/document/DocumentView.hpp>

#include "iscore_git_info.hpp"

#include <QMessageBox>

namespace iscore
{

Presenter::Presenter(View* view, QObject* arg_parent) :
    NamedObject {"Presenter", arg_parent},
    m_view {view},
    m_docManager{*this},
    m_components{},
    m_components_readonly{m_components},
    #ifdef __APPLE__
    m_menubar {new QMenuBar, this}
  #else
    m_menubar {view->menuBar(), this}
  #endif
{
    setupMenus();
    connect(m_view,		&View::insertActionIntoMenubar,
            &m_menubar, &MenubarManager::insertActionIntoMenubar);
    connect(m_view, &View::activeWindowChanged,
            this, &Presenter::on_activeWindowChanged);

    m_view->setPresenter(this);
}

auto getStrongId(const std::vector<Document*>& v)
{
    using namespace std;
    vector<int32_t> ids(v.size());   // Map reduce

    transform(v.begin(),
              v.end(),
              ids.begin(),
              [](const auto elt)
    {
        return * (elt->id().val());
    });

    return Id<DocumentModel>{iscore::id_generator::getNextId(ids)};
}

void Presenter::setupMenus()
{
    ////// File //////
    //// New
    auto newAct = m_menubar.addActionIntoToplevelMenu(
                ToplevelMenuElement::FileMenu,
                FileMenuElement::New,
                [&] () {
        m_docManager.newDocument(getStrongId(m_docManager.documents()),
                    applicationComponents().availableDocuments().front());
    });

    newAct->setShortcut(QKeySequence::New);

    // ----------
    m_menubar.addSeparatorIntoToplevelMenu(
                ToplevelMenuElement::FileMenu,
                FileMenuElement::Separator_Load);

    //// Save and load
    auto openAct = m_menubar.addActionIntoToplevelMenu(
                ToplevelMenuElement::FileMenu,
                FileMenuElement::Load,
                [this]() { m_docManager.loadFile(); });
    openAct->setShortcut(QKeySequence::Open);

    auto saveAct = m_menubar.addActionIntoToplevelMenu(
                ToplevelMenuElement::FileMenu,
                FileMenuElement::Save,
                [this]() { m_docManager.saveDocument(*m_docManager.currentDocument()); });
    saveAct->setShortcut(QKeySequence::Save);

    auto saveAsAct = m_menubar.addActionIntoToplevelMenu(
                ToplevelMenuElement::FileMenu,
                FileMenuElement::SaveAs,
                [this]() { m_docManager.saveDocumentAs(*m_docManager.currentDocument()); });
    saveAsAct->setShortcut(QKeySequence::SaveAs);

    QMenu* fileMenu = m_menubar.menuAt(ToplevelMenuElement::FileMenu);

    fileMenu->addMenu(m_docManager.recentFiles());

    // ----------
    m_menubar.addSeparatorIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                           FileMenuElement::Separator_Export);

    // ----------
    m_menubar.addSeparatorIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                           FileMenuElement::Separator_Quit);


    auto closeAct = m_menubar.addActionIntoToplevelMenu(
                ToplevelMenuElement::FileMenu,
                FileMenuElement::Close,
                [this]() {
        if(auto doc = m_docManager.currentDocument())
            m_docManager.closeDocument(*doc);
    });
    closeAct->setShortcut(QKeySequence::Close);

    m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::FileMenu,
                                        FileMenuElement::Quit,
                                        [&] () {
        m_view->close();
    });

    ////// View //////
    m_menubar.addMenuIntoToplevelMenu(ToplevelMenuElement::ViewMenu,
                                      ViewMenuElement::Windows);

    ////// Settings //////
    m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::SettingsMenu,
                                        SettingsMenuElement::Settings,
                                        std::bind(&SettingsView::exec,
                                                  qobject_cast<Application*> (parent())->settings()->view()));

    ////// About /////
    m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::AboutMenu,
                                        AboutMenuElement::About, [] () {
        QMessageBox::about(nullptr,
                           tr("About i-score"),
                           tr("With love and sweat from the i-score team. \nVersion:\n")
                           + QString("%1.%2.%3-%4 '%5'")
                           .arg(ISCORE_VERSION_MAJOR)
                           .arg(ISCORE_VERSION_MINOR)
                           .arg(ISCORE_VERSION_PATCH)
                           .arg(ISCORE_VERSION_EXTRA)
                           .arg(ISCORE_CODENAME)
                           + tr("\n\nCommit: \n")
                           + QString(ISCORE_XSTR(GIT_COMMIT))); });
}

void Presenter::on_activeWindowChanged()
{
    for(auto ap : m_components.appPlugins)
    {
        ap->on_focusChanged(Qt::ApplicationHidden);
    }
}

bool Presenter::exit()
{
    return m_docManager.closeAllDocuments();
}

View* Presenter::view() const
{
    return m_view;
}

}


