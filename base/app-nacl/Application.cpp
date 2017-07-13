// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Application.hpp"

#include <iscore/tools/std/Optional.hpp>
#include <core/application/ApplicationRegistrar.hpp>
#include <core/document/DocumentBackups.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/undo/Panel/UndoPanelFactory.hpp>
#include <core/presenter/CoreApplicationPlugin.hpp>
#include <core/undo/UndoApplicationPlugin.hpp>
#include <core/view/View.hpp>
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

#include <iscore/tools/IdentifierGeneration.hpp>
#include <algorithm>
#include <vector>

#include <core/application/SafeQApplication.hpp>
#include <iscore/application/ApplicationComponents.hpp>
#include <core/application/ApplicationSettings.hpp>
#include <core/plugin/PluginManager.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <iscore/selection/Selection.hpp>

#include <iscore/model/path/ObjectIdentifier.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore/plugins/settingsdelegate/SettingsDelegateFactory.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <iscore/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <iscore/plugins/panel/PanelDelegate.hpp>

#include <core/document/DocumentModel.hpp>
#include <core/undo/Panel/UndoPanelFactory.hpp>
#include "iscore_git_info.hpp"
namespace iscore {
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
    QCoreApplication::setOrganizationDomain("i-score.org");
    QCoreApplication::setApplicationName("i-score");
    QCoreApplication::setApplicationVersion(
                QString("%1.%2.%3-%4")
                .arg(ISCORE_VERSION_MAJOR)
                .arg(ISCORE_VERSION_MINOR)
                .arg(ISCORE_VERSION_PATCH)
                .arg(ISCORE_VERSION_EXTRA)
                );

    qRegisterMetaType<ObjectIdentifierVector> ("ObjectIdentifierVector");
    qRegisterMetaType<Selection>("Selection");
    qRegisterMetaType<Id<iscore::DocumentModel>>("Id<DocumentModel>");

    QFile stylesheet_file{":/qdarkstyle/qdarkstyle.qss"};
    stylesheet_file.open(QFile::ReadOnly);
    QString stylesheet = QLatin1String(stylesheet_file.readAll());

    qApp->setStyle(QStyleFactory::create("Fusion"));
    qApp->setStyleSheet(stylesheet);
}

}  // namespace iscore


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

    iscore::DocumentBackups::clear();
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
    iscore::setQApplicationSettings(*qApp);

    // MVP
    m_view = new iscore::View{this};
    m_presenter = new iscore::Presenter{m_applicationSettings, m_settings, m_view, this};

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
    if(m_applicationSettings.tryToRestore && iscore::DocumentBackups::canRestoreDocuments())
    {
        m_presenter->documentManager().restoreDocuments(ctx);
    }
    else
    {
        if(!m_presenter->applicationComponents().interfaces<iscore::DocumentDelegateList>().empty())
            m_presenter->documentManager().newDocument(
                        ctx,
                        Id<iscore::DocumentModel>{iscore::random_id_generator::getRandomId()},
                        *m_presenter->applicationComponents().interfaces<iscore::DocumentDelegateList>().begin());
    }

    connect(m_app, &SafeQApplication::fileOpened,
            this, [&] (const QString& file) {
        m_presenter->documentManager().loadFile(ctx, file);
    });
}

void Application::loadPluginData()
{
    auto& ctx = m_presenter->applicationContext();
    iscore::ApplicationRegistrar registrar{
        m_presenter->components(),
                ctx,
                *m_view,
                m_presenter->menuManager(),
                m_presenter->toolbarManager(),
                m_presenter->actionManager()};

    registrar.registerFactory(std::make_unique<iscore::ValidityCheckerList>());
    registrar.registerFactory(std::make_unique<iscore::DocumentDelegateList>());
    auto panels = std::make_unique<iscore::PanelDelegateFactoryList>();
    panels->insert(std::make_unique<iscore::UndoPanelDelegateFactory>());
    registrar.registerFactory(std::move(panels));
    registrar.registerFactory(std::make_unique<iscore::DocumentPluginFactoryList>());
    registrar.registerFactory(std::make_unique<iscore::SettingsDelegateFactoryList>());

    registrar.registerApplicationContextPlugin(new iscore::CoreApplicationPlugin{ctx, *m_presenter});
    registrar.registerApplicationContextPlugin(new iscore::UndoApplicationPlugin{ctx});

    iscore::PluginLoader::loadPlugins(registrar, ctx);
    // Load the settings
    QSettings s;
    for(auto& elt : ctx.interfaces<iscore::SettingsDelegateFactoryList>())
    {
        m_settings.setupSettingsPlugin(s, ctx, elt);
    }

    m_presenter->setupGUI();

    for(iscore::GUIApplicationPlugin* app_plug : ctx.applicationPlugins())
    {
        app_plug->initialize();
    }

    for(auto& panel_fac : context().interfaces<iscore::PanelDelegateFactoryList>())
    {
        registrar.registerPanel(panel_fac);
    }
}
