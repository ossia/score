// This is an open source non-commercial project. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Application.hpp"

#include <score/tools/std/Optional.hpp>
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
#include <QStyle>
#include <QDir>
#include <score/tools/IdentifierGeneration.hpp>
#include <algorithm>
#include <vector>

#include <score/application/GUIApplicationContext.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <core/application/SafeQApplication.hpp>
#include <score/application/ApplicationComponents.hpp>
#include <core/application/ApplicationSettings.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <score/selection/Selection.hpp>

#include <score/model/path/ObjectIdentifier.hpp>
#include <score/model/Identifier.hpp>
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/command/Validity/ValidityChecker.hpp>

#include <core/document/DocumentModel.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>

#include "score_git_info.hpp"

#if __has_include(<QQuickStyle>)
#include <QQuickStyle>
#endif
namespace score {
class DocumentModel;

static void setQApplicationSettings(QApplication &m_app)
{
    QFontDatabase::addApplicationFont(":/APCCourierBold.otf"); // APCCourier-Bold
    QFontDatabase::addApplicationFont(":/Ubuntu-R.ttf"); // Ubuntu


    QFile stylesheet_file{":/qdarkstyle/qdarkstyle.qss"};
    stylesheet_file.open(QFile::ReadOnly);
    QString stylesheet = QLatin1String(stylesheet_file.readAll());

    qApp->setStyle(QStyleFactory::create("Fusion"));
    qApp->setStyleSheet(stylesheet);

    auto pal = qApp->palette();
    pal.setBrush(QPalette::Background, QColor("#001A2024"));
    pal.setBrush(QPalette::Base, QColor("#12171A")); // lineedit bg
    pal.setBrush(QPalette::Button, QColor("#12171A")); // lineedit bg
    pal.setBrush(QPalette::AlternateBase, QColor("#1f2a30")); // alternate bg
    pal.setBrush(QPalette::Highlight, QColor("#3d8ec9")); // tableview bg
    pal.setBrush(QPalette::WindowText, QColor("silver")); // color
    pal.setBrush(QPalette::Text, QColor("silver")); // color
    pal.setBrush(QPalette::ButtonText, QColor("silver")); // color
    pal.setBrush(QPalette::Light, QColor("#666666"));
    pal.setBrush(QPalette::Midlight, QColor("#666666"));
    pal.setBrush(QPalette::Mid, QColor("#666666"));
    pal.setBrush(QPalette::Dark, QColor("#666666"));
    pal.setBrush(QPalette::Shadow, QColor("#666666"));

    QFont f("Ubuntu-R", 9);
    qApp->setFont(f);

    qApp->setPalette(pal);

#if __has_include(<QQuickStyle>)
    QQuickStyle::setStyle(":/desktopqqc2style/Desktop");
#endif
}

}  // namespace score

Application::Application(int& argc, char** argv) :
    QObject {nullptr}
{
    m_instance = this;

    QStringList l;
    for(int i = 0; i < argc; i++)
      l.append(QString::fromUtf8(argv[i]));
    m_applicationSettings.parse(l);

    if(m_applicationSettings.gui)
      m_app = new SafeQApplication{argc, argv};
    else
      m_app = new QCoreApplication{argc, argv};
}

Application::Application(
        const score::ApplicationSettings& appSettings,
        int& argc,
        char** argv) :
    QObject {nullptr},
    m_applicationSettings(appSettings)
{
    m_instance = this;
    if(m_applicationSettings.gui)
      m_app = new SafeQApplication{argc, argv};
    else
      m_app = new QCoreApplication{argc, argv};
}


Application::~Application()
{
    this->setParent(nullptr);
    delete m_view;
    delete m_presenter;

    score::DocumentBackups::clear();
    QCoreApplication::processEvents();
    delete m_app;
}

const score::GUIApplicationContext& Application::context() const
{
  return m_presenter->applicationContext();
}

const score::ApplicationComponents&Application::components() const
{
  return m_presenter->applicationComponents();
}
#define SCORE_DEBUG

void Application::init()
{
#if !defined(SCORE_DEBUG)
  QSplashScreen* splash{};
  if(m_applicationSettings.gui)
  {
    splash = new QSplashScreen{QPixmap{":/score.png"}, Qt::FramelessWindowHint};
    splash->show();
  }
#endif

    this->setObjectName("Application");
    this->setParent(m_app);
#if !defined(__EMSCRIPTEN__)
    m_app->addLibraryPath(m_app->applicationDirPath() + "/plugins");
#endif
#if defined(_MSC_VER)
    QDir::setCurrent(qApp->applicationDirPath());
#endif
    {
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
    }

    // MVP
    if(m_applicationSettings.gui)
    {
      score::setQApplicationSettings(*qApp);
      m_settings.setupView();
      m_view = new score::View{this};
    }

    m_presenter = new score::Presenter{m_applicationSettings, m_settings, m_view, this};

    // Plugins
    loadPluginData();

    // View
    if(m_applicationSettings.gui)
    {
        m_view->show();

#if !defined(SCORE_DEBUG)
        if(splash)
        {
          splash->finish(m_view);
          splash->deleteLater();
        }
#endif
    }

    initDocuments();

    if(m_applicationSettings.gui)
    {
        m_view->sizeChanged(m_view->size());
        m_view->ready();
    }
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
    for(auto plug : ctx.guiApplicationPlugins())
    {
        if(plug->handleStartup())
        {
            return;
        }
    }

    if(auto sqa = dynamic_cast<SafeQApplication*>(m_app))
    {
      connect(sqa, &SafeQApplication::fileOpened,
              this, [&] (const QString& file) {
        m_presenter->documentManager().loadFile(ctx, file);
      });
    }

    // Try to reload if there was a crash
    if(m_applicationSettings.tryToRestore && score::DocumentBackups::canRestoreDocuments())
    {
        m_presenter->documentManager().restoreDocuments(ctx);
    }

    // If nothing was reloaded, open a normal document
    if(m_presenter->documentManager().documents().empty())
    {
        auto& documentKinds = m_presenter->applicationComponents().interfaces<score::DocumentDelegateList>();
        if(!documentKinds.empty() && m_presenter->documentManager().documents().empty())
        {
            m_presenter->documentManager().newDocument(
                        ctx,
                        Id<score::DocumentModel>{score::random_id_generator::getRandomId()},
                        *m_presenter->applicationComponents().interfaces<score::DocumentDelegateList>().begin());
        }
    }

}

void Application::loadPluginData()
{
    auto& ctx = m_presenter->applicationContext();
    score::GUIApplicationRegistrar registrar{
        m_presenter->components(),
                ctx,
                m_presenter->menuManager(),
                m_presenter->toolbarManager(),
                m_presenter->actionManager()};

    GUIApplicationInterface::loadPluginData(ctx, registrar, m_settings, *m_presenter);
}

int Application::exec()
{ return m_app->exec(); }

