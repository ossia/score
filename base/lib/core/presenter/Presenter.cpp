#include <boost/optional/optional.hpp>
#include <core/application/Application.hpp>
#include <core/view/View.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <QAction>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <qnamespace.h>
#include <QObject>

#include <QString>
#include <sys/types.h>
#include <algorithm>
#include <cstdint>
#include <functional>
#include <utility>
#include <vector>

#include <core/document/Document.hpp>
#include "QRecentFilesMenu.h"
#include <iscore/application/ApplicationComponents.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <core/presenter/MenubarManager.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/settings/Settings.hpp>
#include <core/settings/SettingsView.hpp>
#include <iscore/menu/MenuInterface.hpp>
#include <iscore/plugins/customfactory/StringFactoryKey.hpp>
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/widgets/OrderedToolbar.hpp>
#include "iscore_git_info.hpp"

namespace iscore {
class Document;
class DocumentModel;
}  // namespace iscore

namespace iscore
{

Presenter::Presenter(
        iscore::Application& app,
        View* view,
        QObject* arg_parent) :
    NamedObject {"Presenter", arg_parent},
    m_view {view},
    m_docManager{*this},
    m_components{},
    m_components_readonly{m_components},
    m_context{app, m_components_readonly},
    #ifdef __APPLE__
    m_menubar {new QMenuBar, this}
  #else
    m_menubar {view->menuBar(), this}
  #endif
{
    setupMenus();
    connect(m_view,		&View::insertActionIntoMenubar,
            &m_menubar, &MenubarManager::insertActionIntoMenubar);

    m_view->setPresenter(this);
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

#ifdef ISCORE_DEBUG
    m_menubar.addActionIntoToplevelMenu(
                          ToplevelMenuElement::FileMenu,
                          FileMenuElement::SaveCommands,
                          [this] () {m_docManager.saveStack(); });
    m_menubar.addActionIntoToplevelMenu(
                        ToplevelMenuElement::FileMenu,
                        FileMenuElement::LoadCommands,
                [this] () {m_docManager.loadStack();});

#endif

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

bool Presenter::exit()
{
    return m_docManager.closeAllDocuments();
}

View* Presenter::view() const
{
    return m_view;
}

}


