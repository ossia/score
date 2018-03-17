// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Application.hpp"

#include <score/tools/std/Optional.hpp>
#include <core/application/ApplicationRegistrar.hpp>
#include <core/document/DocumentBackups.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/undo/Panel/UndoPanelFactory.hpp>
#include <core/presenter/CoreApplicationPlugin.hpp>
#include <core/undo/UndoApplicationPlugin.hpp>
#include <core/view/Window.hpp>
#include <QByteArray>
#include <QCoreApplication>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QIODevice>
#include <QMetaType>
#include <qnamespace.h>
#include <QPixmap>
#include <QSplashScreen>
#include <QString>
#include <QStringList>
#include <QStyleFactory>
#include <QFileInfo>

#include <score/tools/IdentifierGeneration.hpp>
#include <algorithm>
#include <vector>

#include <core/application/SafeQApplication.hpp>
#include <score/application/ApplicationComponents.hpp>
#include <core/application/ApplicationSettings.hpp>
#include <core/plugin/PluginManager.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <score/selection/Selection.hpp>

#include <score/model/path/ObjectIdentifier.hpp>
#include <score/model/Identifier.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateFactory.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/plugins/panel/PanelDelegate.hpp>

#include <core/document/DocumentModel.hpp>
#include <core/undo/Panel/UndoPanelFactory.hpp>
#include "score_git_info.hpp"
namespace score {
class DocumentModel;

static QApplication* make_application(
        int& argc,
        char** argv)
{
#if defined(__native_client__)
    return qApp;
#else
    return new SafeQApplication{argc, argv};
#endif
}

static void setQApplicationSettings(QApplication &m_app)
{
    QFontDatabase::addApplicationFont(":/APCCourierBold.otf"); // APCCourier-Bold
    QFontDatabase::addApplicationFont(":/Ubuntu-R.ttf"); // Ubuntu

    QCoreApplication::setOrganizationName("OSSIA");
    QCoreApplication::setOrganizationDomain("ossia.io");
    QCoreApplication::setApplicationName("score");
    QCoreApplication::setApplicationVersion(
                QString("%1.%2.%3-%4")
                .arg(SCORE_VERSION_MAJOR)
                .arg(SCORE_VERSION_MINOR)
                .arg(SCORE_VERSION_PATCH)
                .arg(SCORE_VERSION_EXTRA)
                );

    qRegisterMetaType<ObjectIdentifierVector> ("ObjectIdentifierVector");
    qRegisterMetaType<Selection>("Selection");
    qRegisterMetaType<Id<score::DocumentModel>>("Id<DocumentModel>");

    QFile stylesheet_file{":/qdarkstyle/qdarkstyle.qss"};
    stylesheet_file.open(QFile::ReadOnly);
    QString stylesheet = QLatin1String(stylesheet_file.readAll());

    qApp->setStyle(QStyleFactory::create("Fusion"));
    qApp->setStyleSheet(stylesheet);
}

}  // namespace score


Application::Application(int& argc, char** argv) :
    NamedObject {"Application", nullptr},
    m_app{new SafeQApplication{argc, argv}}
{
    m_instance = this;
}

Application::Application(
        const ApplicationSettings& appSettings,
        int& argc,
        char** argv) :
    NamedObject {"Application", nullptr},
    m_app{new SafeQApplication{argc, argv}},
    m_applicationSettings(appSettings)
{
    m_instance = this;
}


Application::~Application()
{
    this->setParent(nullptr);
    delete m_view;
    delete m_presenter;

    score::DocumentBackups::clear();
    QApplication::processEvents();
    delete m_app;
}

const ApplicationContext& Application::context() const
{
    return m_presenter->applicationContext();
}


void Application::init()
{
    this->setObjectName("Application");
    this->setParent(qApp);
    score::setQApplicationSettings(*qApp);

    // MVP
    m_view = new score::View{this};
    m_presenter = new score::Presenter{m_applicationSettings, m_settings, m_view, this};

    // Plugins
    loadPluginData();

    // View
    if(m_applicationSettings.gui)
    {
        m_view->show();
    }

    initDocuments();
}

void Application::initDocuments()
{
    auto& ctx = m_presenter->applicationContext();
    if(!m_applicationSettings.loadList.empty())
    {
        for(const auto& doc : m_applicationSettings.loadList)
            m_presenter->documentManager().loadFile(ctx, doc);
    }

    // The plug-ins have the ability to override the boot process.
    for(auto plug : ctx.applicationPlugins())
    {
        if(plug->handleStartup())
        {
            return;
        }
    }

    // Try to reload if there was a crash
    if(m_applicationSettings.tryToRestore && score::DocumentBackups::canRestoreDocuments())
    {
        m_presenter->documentManager().restoreDocuments(ctx);
    }
    else
    {
        if(!m_presenter->applicationComponents().interfaces<score::DocumentDelegateList>().empty())
            m_presenter->documentManager().newDocument(
                        ctx,
                        Id<score::DocumentModel>{score::random_id_generator::getRandomId()},
                        *m_presenter->applicationComponents().interfaces<score::DocumentDelegateList>().begin());
    }

    connect(m_app, &SafeQApplication::fileOpened,
            this, [&] (const QString& file) {
        m_presenter->documentManager().loadFile(ctx, file);
    });
}

void Application::loadPluginData()
{
    auto& ctx = m_presenter->applicationContext();
    score::ApplicationRegistrar registrar{
        m_presenter->components(),
                ctx,
                *m_view,
                m_presenter->menuManager(),
                m_presenter->toolbarManager(),
                m_presenter->actionManager()};

    registrar.registerFactory(std::make_unique<score::ValidityCheckerList>());
    registrar.registerFactory(std::make_unique<score::DocumentDelegateList>());
    auto panels = std::make_unique<score::PanelDelegateFactoryList>();
    panels->insert(std::make_unique<score::UndoPanelDelegateFactory>());
    registrar.registerFactory(std::move(panels));
    registrar.registerFactory(std::make_unique<score::DocumentPluginFactoryList>());
    registrar.registerFactory(std::make_unique<score::SettingsDelegateFactoryList>());

    registrar.registerApplicationContextPlugin(new score::CoreApplicationPlugin{ctx, *m_presenter});
    registrar.registerApplicationContextPlugin(new score::UndoApplicationPlugin{ctx});

    score::PluginLoader::loadPlugins(registrar, ctx);
    // Load the settings
    QSettings s;
    for(auto& elt : ctx.interfaces<score::SettingsDelegateFactoryList>())
    {
        m_settings.setupSettingsPlugin(s, ctx, elt);
    }

    m_presenter->setupGUI();

    for(score::GUIApplicationPlugin* app_plug : ctx.applicationPlugins())
    {
        app_plug->initialize();
    }

    for(auto& panel_fac : context().interfaces<score::PanelDelegateFactoryList>())
    {
        registrar.registerPanel(panel_fac);
    }
}
