#include "Application.hpp"

#include <iscore/tools/std/Optional.hpp>
#include <core/application/ApplicationRegistrar.hpp>
#include <core/document/DocumentBackups.hpp>
#include <core/presenter/Presenter.hpp>
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
#include <QDir>
#include <iscore/tools/IdentifierGeneration.hpp>
#include <algorithm>
#include <vector>

#include <iscore/application/GUIApplicationContext.hpp>
#include <iscore/plugins/application/GUIApplicationContextPlugin.hpp>
#include <core/application/SafeQApplication.hpp>
#include <iscore/application/ApplicationComponents.hpp>
#include <core/application/ApplicationSettings.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <iscore/selection/Selection.hpp>

#include <iscore/model/path/ObjectIdentifier.hpp>
#include <iscore/model/Identifier.hpp>
#include <iscore/plugins/panel/PanelDelegate.hpp>
#include <iscore/command/Validity/ValidityChecker.hpp>

#include <core/document/DocumentModel.hpp>
#include <iscore/plugins/documentdelegate/DocumentDelegateFactory.hpp>

#include "iscore_git_info.hpp"

namespace iscore {
class DocumentModel;

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

    QFile stylesheet_file{":/qdarkstyle/qdarkstyle.qss"};
    stylesheet_file.open(QFile::ReadOnly);
    QString stylesheet = QLatin1String(stylesheet_file.readAll());

    qApp->setStyle(QStyleFactory::create("Fusion"));
    qApp->setStyleSheet(stylesheet);
}

}  // namespace iscore

Application::Application(int& argc, char** argv) :
    QObject {nullptr},
    m_app{new SafeQApplication{argc, argv}}
{
    m_instance = this;

    m_applicationSettings.parse();
}

Application::Application(
        const iscore::ApplicationSettings& appSettings,
        int& argc,
        char** argv) :
    QObject {nullptr},
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

const iscore::GUIApplicationContext& Application::context() const
{
  return m_presenter->applicationContext();
}

const iscore::ApplicationComponents&Application::components() const
{
  return m_presenter->applicationComponents();
}


void Application::init()
{
#if !defined(ISCORE_DEBUG)
    QSplashScreen splash{QPixmap{":/i-score.png"}, Qt::FramelessWindowHint};
    if(m_applicationSettings.gui)
        splash.show();
#endif

    this->setObjectName("Application");
    this->setParent(qApp);
    qApp->addLibraryPath(qApp->applicationDirPath() + "/plugins");
#if defined(_MSC_VER)
    QDir::setCurrent(qApp->applicationDirPath());
#endif
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

#if !defined(ISCORE_DEBUG)
        splash.finish(m_view);
#endif
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
        auto& documentKinds = m_presenter->applicationComponents().interfaces<iscore::DocumentDelegateList>();
        if(!documentKinds.empty() && m_presenter->documentManager().documents().empty())
        {
            m_presenter->documentManager().newDocument(
                        ctx,
                        Id<iscore::DocumentModel>{iscore::random_id_generator::getRandomId()},
                        *m_presenter->applicationComponents().interfaces<iscore::DocumentDelegateList>().begin());
        }
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

    GUIApplicationInterface::loadPluginData(ctx, registrar, m_settings, *m_presenter);
}

int Application::exec()
    { return m_app->exec(); }

