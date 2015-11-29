#include <boost/optional/optional.hpp>
#include <core/application/Application.hpp>
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

#include "SafeQApplication.hpp"
#include <core/application/ApplicationComponents.hpp>
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
}  // namespace iscore

static Application* application_instance = nullptr;


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
    ::application_instance = this;

    m_applicationSettings.parse();

    init();
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
    ::application_instance = this;

    init();
}


Application::~Application()
{
    this->setParent(nullptr);
    delete m_presenter;

    DocumentBackups::clear();
    delete m_app;
}

void Application::init()
{
#if !defined(ISCORE_DEBUG)
    QPixmap logo{":/i-score.png"};
    QSplashScreen splash{logo, Qt::FramelessWindowHint};
    if(m_applicationSettings.gui)
        splash.show();
#endif

    QFontDatabase::addApplicationFont(":/APCCourierBold.otf"); // APCCourier-Bold
    QFontDatabase::addApplicationFont(":/Ubuntu-R.ttf"); // Ubuntu
    m_app->setFont(QFont{"Ubuntu", 10, QFont::Normal});
    this->setParent(m_app);
    this->setObjectName("Application");

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

    // Settings
    m_settings = std::make_unique<Settings> (this);

    // MVP
    m_view = new View{this};
    m_presenter = new Presenter{m_view, this};

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

    if(!m_applicationSettings.loadList.empty())
    {
        for(const auto& doc : m_applicationSettings.loadList)
            m_presenter->documentManager().loadFile(doc);

        if(!m_presenter->documentManager().documents().empty())
        {
            if(m_applicationSettings.autoplay)
            {
                // TODO find a better way, most likely by allowing plug-ins to have
                // command line arguments, too. Eww.
                emit autoplay();
            }
            return; // A document was loaded correctly
        }
    }

    // Try to reload if there was a crash
    if(m_applicationSettings.tryToRestore && DocumentBackups::canRestoreDocuments())
    {
        m_presenter->documentManager().restoreDocuments();
    }
    else
    {
        if(!m_presenter->applicationComponents().availableDocuments().empty())
            m_presenter->documentManager().newDocument(
                        Id<DocumentModel>{iscore::random_id_generator::getRandomId()}, // TODO crashes if loaded twice by chance
                        m_presenter->applicationComponents().availableDocuments().front());
    }
}

Application &Application::instance()
{
    return *application_instance;
}

void Application::loadPluginData()
{
    // TODO finish to do this properly.
    ApplicationRegistrar registrar{m_presenter->components(), *this};

    m_pluginManager.reloadPlugins(registrar);
    registrar.registerApplicationContextPlugin(new UndoApplicationPlugin{*this, m_presenter});
    registrar.registerPanel(new UndoPanelFactory);

    std::sort(m_presenter->toolbars().begin(), m_presenter->toolbars().end());
    for(auto& toolbar : m_presenter->toolbars())
    {
        m_view->addToolBar(toolbar.bar);
    }
}

