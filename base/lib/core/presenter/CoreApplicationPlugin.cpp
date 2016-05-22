#include "CoreApplicationPlugin.hpp"
#include <iscore_git_info.hpp>
#include <core/view/View.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateFactoryInterface.hpp>
#include <QMessageBox>
#include <QRecentFilesMenu.h>
#include <QMessageAuthenticationCode>
namespace iscore
{

CoreApplicationPlugin::CoreApplicationPlugin(const ApplicationContext& app, Presenter& pres):
    GUIApplicationContextPlugin{app},
    m_presenter{pres}
{

}

void CoreApplicationPlugin::newDocument()
{
    m_presenter.m_docManager.newDocument(
                context,
                getStrongId(m_presenter.m_docManager.documents()),
                *context.components.factory<iscore::DocumentDelegateList>().begin());

}

void CoreApplicationPlugin::load()
{
    m_presenter.m_docManager.loadFile(context);
}

void CoreApplicationPlugin::save()
{
    m_presenter.m_docManager.saveDocument(*m_presenter.m_docManager.currentDocument());
}

void CoreApplicationPlugin::saveAs()
{
    m_presenter.m_docManager.saveDocumentAs(*m_presenter.m_docManager.currentDocument());
}

void CoreApplicationPlugin::quit()
{
    m_presenter.m_view->close();
}

void CoreApplicationPlugin::about()
{
    auto version_text =
            QString("%1.%2.%3-%4 '%5'\n\n")
            .arg(ISCORE_VERSION_MAJOR)
            .arg(ISCORE_VERSION_MINOR)
            .arg(ISCORE_VERSION_PATCH)
            .arg(ISCORE_VERSION_EXTRA)
            .arg(ISCORE_CODENAME);

    QString commit{GIT_COMMIT};
    if(!commit.isEmpty())
    {
        version_text += tr("Commit: \n%1\n").arg(commit);
    }

    QMessageBox::about(nullptr,
                       tr("About i-score"),
                       tr("With love and sweat from the i-score team. \nVersion:\n")
                       + version_text);
}

GUIApplicationContextPlugin::GUIElements CoreApplicationPlugin::makeGUIElements()
{
    std::vector<Menu> menus;
    menus.reserve(8);
    auto file = new QMenu{tr("&File")};
    auto edit = new QMenu{tr("&Edit")};
    auto object = new QMenu{tr("&Object")};
    auto play = new QMenu{tr("&Play")};
    auto tool = new QMenu{tr("&Tool")};
    auto view = new QMenu{tr("&View")};
    auto settings = new QMenu{tr("&Settings")};
    auto about = new QMenu{tr("&About")};
    menus.emplace_back(file, StringKey<Menu>{"File"}, 0);
    menus.emplace_back(edit, StringKey<Menu>{"Edit"}, 1);
    menus.emplace_back(object, StringKey<Menu>{"Object"}, 2);
    menus.emplace_back(play, StringKey<Menu>{"Play"}, 3);
    menus.emplace_back(tool, StringKey<Menu>{"Tool"}, 4);
    menus.emplace_back(view, StringKey<Menu>{"View"}, 5);
    menus.emplace_back(settings, StringKey<Menu>{"Settings"}, 6);

    // Menus are by default at int_max - 1 so that they will be sorted before
    menus.emplace_back(about, StringKey<Menu>{"About"}, std::numeric_limits<int>::max());

    auto export_menu = new QMenu{tr("&Export")};
    menus.emplace_back(export_menu, StringKey<Menu>{"Export"});

    std::vector<Action> actions;
    ////// File //////
    // New
    // ---
    // Load
    // Recent
    // Save
    // Save As
    // ---
    // Export as
    // ---
    // Quit

    {
        auto new_doc = new QAction(tr("&New"), m_presenter.view());
        connect(new_doc, &QAction::triggered, this, &CoreApplicationPlugin::newDocument);
        file->addAction(new_doc);
        actions.emplace_back(Action(new_doc, "New", "Common", Action::EnablementContext::Application, QKeySequence::New));
    }

    file->addSeparator();


    {
        auto load_doc = new QAction(tr("&Load"), m_presenter.view());
        connect(load_doc, &QAction::triggered, this, &CoreApplicationPlugin::load);
        actions.emplace_back(load_doc, "Load", "Common", Action::EnablementContext::Application, QKeySequence::Open);
        file->addAction(load_doc);
    }

    file->addMenu(m_presenter.m_docManager.recentFiles());

    {
        auto save_doc = new QAction(tr("&Save"), m_presenter.view());
        connect(save_doc, &QAction::triggered, this, &CoreApplicationPlugin::save);
        actions.emplace_back(save_doc, "Save", "Common", Action::EnablementContext::Document, QKeySequence::Save);
        file->addAction(save_doc);
    }

    {
        auto saveas_doc = new QAction(tr("Save &As..."), m_presenter.view());
        connect(saveas_doc, &QAction::triggered, this, &CoreApplicationPlugin::saveAs);
        actions.emplace_back(saveas_doc, "SaveAs", "Common", Action::EnablementContext::Document, QKeySequence::SaveAs);
        file->addAction(saveas_doc);
    }

    file->addSeparator();

    file->addMenu(export_menu);

    file->addSeparator();

    auto quit_act = new QAction(tr("&Quit"), m_presenter.view());
    connect(quit_act, &QAction::triggered, this, &CoreApplicationPlugin::quit);
    actions.emplace_back(quit_act, "Quit", "Common", Action::EnablementContext::Application, QKeySequence::Quit);
    file->addAction(quit_act);

    ////// Settings //////


    return std::make_tuple(menus,  std::vector<Toolbar>{}, actions);
/*
    auto newAct = m_menubar.addActionIntoToplevelMenu(
                      ToplevelMenuElement::FileMenu,
                      FileMenuElement::New,
                      [&] () {
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
                       [this]() { m_docManager.loadFile(context); });
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
            m_docManager.closeDocument(context, *doc);
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
                [this] () { m_docManager.loadStack(context); });

#endif

    ////// View //////
    m_menubar.addMenuIntoToplevelMenu(ToplevelMenuElement::ViewMenu,
                                      ViewMenuElement::Windows);

    ////// Settings //////
    m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::SettingsMenu,
                                        SettingsMenuElement::Settings,
                                        [this] () { m_settings.view().exec(); });
    ////// About /////
    m_menubar.addActionIntoToplevelMenu(ToplevelMenuElement::AboutMenu,
                                        AboutMenuElement::About, [] () );
                                        **/
}

}
