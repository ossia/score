#include "Application.hpp"

#include <boost/optional/optional.hpp>
#include <core/application/ApplicationRegistrar.hpp>
#include <core/document/DocumentBackups.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/undo/Panel/UndoPanelFactory.hpp>
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

using namespace iscore;
#include <iscore/tools/SettableIdentifierGeneration.hpp>
#include <algorithm>
#include <vector>

#include <core/application/SafeQApplication.hpp>
#include <iscore/application/ApplicationComponents.hpp>
#include <core/application/ApplicationSettings.hpp>
#include <core/plugin/PluginManager.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <core/settings/Settings.hpp>
#include <iscore/selection/Selection.hpp>
#include <iscore/tools/NamedObject.hpp>
#include <iscore/tools/ObjectIdentifier.hpp>
#include <iscore/tools/SettableIdentifier.hpp>
#include <iscore/widgets/OrderedToolbar.hpp>
#include "iscore_git_info.hpp"

namespace iscore {
class DocumentModel;

static void setQApplicationSettings(QApplication &m_app)
{
    QFontDatabase::addApplicationFont(":/APCCourierBold.otf"); // APCCourier-Bold
    QFontDatabase::addApplicationFont(":/Ubuntu-R.ttf"); // Ubuntu
    m_app.setFont(QFont{"Ubuntu", 10, QFont::Normal});

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

    QFile stylesheet_file{":/qdarkstyle/qdarkstyle.qss"};
    stylesheet_file.open(QFile::ReadOnly);
    QString stylesheet = QLatin1String(stylesheet_file.readAll());

    qApp->setStyle(QStyleFactory::create("Fusion"));
    qApp->setStyleSheet(stylesheet);
}

}  // namespace iscore

#ifdef ISCORE_DEBUG

static void myMessageOutput(
        QtMsgType type,
        const QMessageLogContext &context,
        const QString &msg)
{
    auto basename_arr = QFileInfo(context.file).baseName().toUtf8();
    auto basename = basename_arr.constData();

    QByteArray localMsg = msg.toLocal8Bit();
    switch (type) {
    case QtDebugMsg:
        fprintf(stderr, "Debug: %s (%s:%u, %s)\n", localMsg.constData(), basename, context.line, context.function);
        break;

#if (QT_VERSION >= QT_VERSION_CHECK(5, 5, 0))
    case QtInfoMsg:
        fprintf(stderr, "Info: %s (%s:%u, %s)\n", localMsg.constData(), basename, context.line, context.function);
        break;
#endif
    case QtWarningMsg:
        fprintf(stderr, "Warning: %s (%s:%u, %s)\n", localMsg.constData(), basename, context.line, context.function);
        break;
    case QtCriticalMsg:
        fprintf(stderr, "Critical: %s (%s:%u, %s)\n", localMsg.constData(), basename, context.line, context.function);
        break;
    case QtFatalMsg:
        fprintf(stderr, "Fatal: %s (%s:%u, %s)\n", localMsg.constData(), basename, context.line, context.function);
        ISCORE_BREAKPOINT;
        std::terminate();
    }
}

#endif

Application::Application(int& argc, char** argv) :
    NamedObject {"Application", nullptr}
{
#ifdef ISCORE_DEBUG
    qInstallMessageHandler(myMessageOutput);
#endif
    // Application
    // Crashes if put in member initialization list... :(
    m_app = new SafeQApplication{argc, argv};
    m_instance = this;

#if !defined(__native_client__)
    m_applicationSettings.parse();
#endif
}

Application::Application(
        const ApplicationSettings& appSettings,
        int& argc,
        char** argv) :
    NamedObject {"Application", nullptr},
    m_applicationSettings(appSettings)
{
#ifdef ISCORE_DEBUG
    qInstallMessageHandler(myMessageOutput);
#endif
    // Application
    // Crashes if put in member initialization list... :(
    m_app = new SafeQApplication{argc, argv};
    m_instance = this;
}


Application::~Application()
{
    this->setParent(nullptr);
    delete m_view;
    delete m_presenter;

    DocumentBackups::clear();
    delete m_app;
}

const ApplicationContext& Application::context() const
{
    return m_presenter->applicationContext();
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
    setQApplicationSettings(*qApp);

    // Settings
    m_settings = std::make_unique<Settings> (this);

    // MVP
    m_view = new View{this};
    m_presenter = new Presenter{m_applicationSettings, m_view, this};

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
    for(auto plug : ctx.components.applicationPlugins())
    {
        if(plug->handleStartup())
        {
            return;
        }
    }

    // Try to reload if there was a crash
    if(m_applicationSettings.tryToRestore && DocumentBackups::canRestoreDocuments())
    {
        m_presenter->documentManager().restoreDocuments(ctx);
    }
    else
    {
        if(!m_presenter->applicationComponents().availableDocuments().empty())
            m_presenter->documentManager().newDocument(
                        ctx,
                        Id<DocumentModel>{iscore::random_id_generator::getRandomId()}, // TODO crashes if loaded twice by chance
                        m_presenter->applicationComponents().availableDocuments().front());
    }
}

void Application::loadPluginData()
{
    auto& ctx = m_presenter->applicationContext();
    ApplicationRegistrar registrar{
        m_presenter->components(),
                ctx,
                *m_view,
                *m_settings,
                m_presenter->menuBar(),
                m_presenter->toolbars(),
                m_presenter};

    PluginLoader::loadPlugins(registrar, ctx);

    registrar.registerApplicationContextPlugin(new UndoApplicationPlugin{ctx, m_presenter});
    registrar.registerPanel(new UndoPanelFactory);

    std::sort(m_presenter->toolbars().begin(), m_presenter->toolbars().end());
    for(auto& toolbar : m_presenter->toolbars())
    {
        m_view->addToolBar(toolbar.bar);
    }
}
