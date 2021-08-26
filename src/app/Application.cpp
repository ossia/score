// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Application.hpp"
#include <score/command/Validity/ValidityChecker.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/ObjectIdentifier.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/selection/Selection.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/widgets/Pixmap.hpp>
#include <score/model/Skin.hpp>

#include <core/application/ApplicationRegistrar.hpp>
#include <core/application/OpenDocumentsFile.hpp>
#include <core/application/SafeQApplication.hpp>
#include <core/document/DocumentBackups.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/view/Window.hpp>

#include <ossia-qt/qt_logger.hpp>
#include <ossia/context.hpp>

#include <QFontDatabase>
#include <QDesktopServices>
#include <QKeyEvent>
#include <QUrl>
#include <QOpenGLContext>
#include <QPushButton>
#include <QLabel>
#include <QDir>
#include <QPainter>
#include <QStandardPaths>
#include <qobjectdefs.h>
#include <qconfig.h>
#if defined(QT_FEATURE_thread)
#if QT_FEATURE_thread == 1
#include <QThreadPool>
#endif
#endif
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>

#if defined(SCORE_STATIC_PLUGINS)
#include <score_static_plugins.hpp>
#endif

#include <wobjectimpl.h>
W_OBJECT_IMPL(Application)

#if defined(SCORE_SOURCE_DIR)
#include <QFileSystemWatcher>
#endif
#if defined(SCORE_STATIC_PLUGINS)
int qInitResources_score();
#endif

#if !defined(SCORE_DEBUG) && !defined(__EMSCRIPTEN__)
#define SCORE_SPLASH_SCREEN 1
#endif
#include <phantom/phantomstyle.h>

#if defined(SCORE_SPLASH_SCREEN)
#include "StartScreen.hpp"
#else
namespace score
{
class StartScreen : public QWidget
{

};
}
#endif

namespace score
{
class DocumentModel;

static void setQApplicationSettings(QApplication& m_app)
{
#if defined(SCORE_STATIC_PLUGINS)
  qInitResources_score();
#endif

  QFontDatabase::addApplicationFont(":/APCCourierBold.otf"); // APCCourier-Bold

  QFontDatabase::addApplicationFont(":/Ubuntu-R.ttf");       // Ubuntu Regular
  QFontDatabase::addApplicationFont(":/Ubuntu-B.ttf");       // Ubuntu Bold
  QFontDatabase::addApplicationFont(":/Ubuntu-L.ttf");       // Ubuntu Light
  QFontDatabase::addApplicationFont(":/Catamaran-Regular.ttf"); // Catamaran Regular
  QFontDatabase::addApplicationFont(":/Montserrat-Regular.ttf"); // Montserrat
  QFontDatabase::addApplicationFont(":/Montserrat-SemiBold.ttf"); // Montserrat
  QFontDatabase::addApplicationFont(":/Montserrat-Light.ttf"); // Montserrat

  // Source Code Pro
  QFontDatabase::addApplicationFont(":/fonts/sourcecodepro/SourceCodePro-Black.ttf");
  QFontDatabase::addApplicationFont(":/fonts/sourcecodepro/SourceCodePro-BlackIt.ttf");
  QFontDatabase::addApplicationFont(":/fonts/sourcecodepro/SourceCodePro-Bold.ttf");
  QFontDatabase::addApplicationFont(":/fonts/sourcecodepro/SourceCodePro-BoldIt.ttf");
  QFontDatabase::addApplicationFont(":/fonts/sourcecodepro/SourceCodePro-ExtraLight.ttf");
  QFontDatabase::addApplicationFont(":/fonts/sourcecodepro/SourceCodePro-ExtraLightIt.ttf");
  QFontDatabase::addApplicationFont(":/fonts/sourcecodepro/SourceCodePro-Light.ttf");
  QFontDatabase::addApplicationFont(":/fonts/sourcecodepro/SourceCodePro-LightIt.ttf");
  QFontDatabase::addApplicationFont(":/fonts/sourcecodepro/SourceCodePro-It.ttf");
  QFontDatabase::addApplicationFont(":/fonts/sourcecodepro/SourceCodePro-Medium.ttf");
  QFontDatabase::addApplicationFont(":/fonts/sourcecodepro/SourceCodePro-MediumIt.ttf");
  QFontDatabase::addApplicationFont(":/fonts/sourcecodepro/SourceCodePro-Semibold.ttf");
  QFontDatabase::addApplicationFont(":/fonts/sourcecodepro/SourceCodePro-SemiboldIt.ttf");

  m_app.setStyle(new PhantomStyle);

  auto pal = qApp->palette();
  pal.setBrush(QPalette::Window, QColor("#222222"));//#1A2024"));
  pal.setBrush(QPalette::Base, QColor("#161514"));//12171A"));
  pal.setBrush(QPalette::AlternateBase, QColor("#1e1d1c"));//1f2a30")); // alternate bg
  pal.setBrush(QPalette::Highlight, QColor{"#9062400a"});
  pal.setBrush(QPalette::HighlightedText, QColor("#FDFDFD"));
  pal.setBrush(QPalette::WindowText, QColor("silver"));
  pal.setBrush(QPalette::Text, QColor("#d0d0d0"));

  pal.setBrush(QPalette::Button, QColor("#1d1c1a"));
  pal.setBrush(QPalette::ButtonText, QColor("#f0f0f0"));

  pal.setBrush(QPalette::ToolTipBase, QColor("#161514"));
  pal.setBrush(QPalette::ToolTipText, QColor("silver"));

  pal.setBrush(QPalette::Midlight, QColor{"#62400a"});
  pal.setBrush(QPalette::Light, QColor{"#c58014"});
  pal.setBrush(QPalette::Mid, QColor("#252930"));

//  pal.setBrush(QPalette::Dark, QColor("#808080"));
 // pal.setBrush(QPalette::Shadow, QColor("#666666"));

  constexpr const int defaultFontSize = 10;

  QFont f("Ubuntu", defaultFontSize);
  f.setHintingPreference(QFont::HintingPreference::PreferVerticalHinting);
  qApp->setFont(f);

  qApp->setPalette(pal);
}

} // namespace score

namespace {
bool runningUnderAnUISession() noexcept
{
#if __linux__
  return
      !qgetenv("DISPLAY").isEmpty() ||
      !qgetenv("WAYLAND_DISPLAY").isEmpty() ||
      (qgetenv("XDG_SESSION_TYPE") != "tty") ||
      (qgetenv("QT_QPA_PLATFORM").contains("gl"))
      ;
#else
  // Win32 and macOS always have a graphical session
  return true;
#endif
}

QCoreApplication* createApplication(const score::ApplicationSettings& set, int& argc, char** argv)
{
  if (set.gui)
  {
    return new SafeQApplication{argc, argv};
  }
  else
  {
    if(runningUnderAnUISession())
      return new QGuiApplication{argc, argv};
    else
      return new QCoreApplication{argc, argv};
  }
}
}

Application::Application(int& argc, char** argv) : QObject{nullptr}
{
  m_instance = this;

  QStringList l;
  for (int i = 0; i < argc; i++)
    l.append(QString::fromUtf8(argv[i]));
  m_applicationSettings.parse(l, argc, argv);

  QCoreApplication::setOrganizationName("ossia");
  QCoreApplication::setOrganizationDomain("ossia.io");
  QCoreApplication::setApplicationName("score");

#if !defined(__EMSCRIPTEN__)
  QSettings s;
  if(s.contains("Skin/Zoom"))
  {
    double zoom = s.value("Skin/Zoom").toDouble();
    if(zoom != 1.)
    {
      zoom = qBound(1.0, zoom, 10.);
      qputenv("QT_SCALE_FACTOR", QString("%1").arg(zoom).toLatin1());
    }
  }
#endif

  m_app = createApplication(m_applicationSettings, argc, argv);

}

Application::Application(
    const score::ApplicationSettings& appSettings, int& argc, char** argv)
    : QObject{nullptr}, m_applicationSettings(appSettings)
{
  m_instance = this;

  QCoreApplication::setOrganizationName("ossia");
  QCoreApplication::setOrganizationDomain("ossia.io");
  QCoreApplication::setApplicationName("score");

#if !defined(__EMSCRIPTEN__)
  QSettings s;
  if(s.contains("Skin/Zoom"))
  {
    double zoom = s.value("Skin/Zoom").toDouble();
    if(zoom != 1.)
    {
      zoom = qBound(1.0, zoom, 10.);
      qputenv("QT_SCALE_FACTOR", QString("%1").arg(zoom).toLatin1());
    }
  }
#endif

  m_app = createApplication(m_applicationSettings, argc, argv);
}

Application::~Application()
{
  this->setParent(nullptr);
  delete m_view;
  delete m_presenter;
  delete m_startScreen;

  score::DocumentBackups::clear();
  QCoreApplication::processEvents();
  delete m_app;
}

const score::GUIApplicationContext& Application::context() const
{
  return m_presenter->applicationContext();
}

const score::ApplicationComponents& Application::components() const
{
  return m_presenter->applicationComponents();
}


void Application::init()
{
  if(m_applicationSettings.gui && m_applicationSettings.opengl)
  {
    auto platform = QGuiApplication::platformName();
    if(platform.contains(QStringLiteral("wayland")))
    {
      m_applicationSettings.opengl = false;
    }
    else
    {
#if !defined(__arm__)
      QOpenGLContext ctx;
      m_applicationSettings.opengl = ctx.create();
#else
      m_applicationSettings.opengl = true;
#endif
    }
  }
#if defined(SCORE_STATIC_PLUGINS)
  score_init_static_plugins();
#endif

  std::vector<spdlog::sink_ptr> v;
  /*
  try {
    v.push_back(std::make_shared<spdlog::sinks::stderr_sink_mt>());
  } catch (...) { }
  */
  try {
    v.push_back(std::make_shared<ossia::qt::log_sink>());
  } catch (...) { }

  ossia::context context{std::move(v)};
  ossia::logger().set_level(spdlog::level::debug);

  score::setQApplicationMetadata();

  this->setObjectName("Application");
  this->setParent(m_app);
#if !defined(__EMSCRIPTEN__)
  m_app->addLibraryPath(m_app->applicationDirPath() + "/plugins");
#endif
#if defined(_MSC_VER)
  QDir::setCurrent(qApp->applicationDirPath());
  auto path = qgetenv("PATH");
  path += ";" + QCoreApplication::applicationDirPath();
  path += ";" + QCoreApplication::applicationDirPath() + "/plugins";
  qputenv("PATH", path);
  SetDllDirectoryW((wchar_t*)QCoreApplication::applicationDirPath().utf16());
  const QString dir2 = (QCoreApplication::applicationDirPath() + "/plugins");
  SetDllDirectoryW((wchar_t*)dir2.utf16());
#endif

  // MVP
  if (m_applicationSettings.gui)
  {
    score::setQApplicationSettings(*qApp);
    m_settings.setupView();
    //m_projectSettings.setupView();
    m_view = new score::View{this};
  }

  m_presenter = new score::Presenter{m_applicationSettings, m_settings,
                                     m_projectSettings, m_view, this};
  // Plugins
  GUIApplicationInterface::loadPluginData(m_settings, *m_presenter);

  // View
  if (m_applicationSettings.gui)
  {
#if !defined(__EMSCRIPTEN__)
    m_view->show();
#else
    m_view->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    m_view->showFullScreen();
#endif
  }

#if defined(__APPLE__)
  {
    if (m_applicationSettings.gui)
    {
      auto sqa = safe_cast<SafeQApplication*>(m_app);
      if(auto file = sqa->fileToOpen; QFile::exists(file))
      {
        m_applicationSettings.loadList.push_back(file);
      }
    }
  }
#endif

#if defined(SCORE_SPLASH_SCREEN)
  if (m_applicationSettings.gui && m_applicationSettings.loadList.empty())
  {
    m_startScreen = new score::StartScreen{this->context().docManager.recentFiles()};
    m_startScreen->show();

    auto& ctx = m_presenter->applicationContext();
    connect(
          m_startScreen, &score::StartScreen::openNewDocument, this, [&]() {
      m_startScreen->close();
      openNewDocument();
    });
    connect(
          m_startScreen, &score::StartScreen::openFile, this, [&](const QString& file) {
      m_startScreen->close();
      m_presenter->documentManager().loadFile(ctx, file);
    });
    connect(
          m_startScreen, &score::StartScreen::openFileDialog, this, [&]() {
      m_startScreen->close();
      m_presenter->documentManager().loadFile(ctx);
    });
  }
#endif

  if (m_applicationSettings.gui)
  {
    m_view->sizeChanged(m_view->size());
    m_view->ready();
  }

  QTimer::singleShot(10, [&] { initDocuments(); });
}

void Application::initDocuments()
{
  auto& ctx = m_presenter->applicationContext();
  if (!m_applicationSettings.loadList.empty())
  {
    for (const auto& doc : m_applicationSettings.loadList)
      m_presenter->documentManager().loadFile(ctx, doc);
  }

  // The plug-ins have the ability to override the boot process.
  for (auto plug : ctx.guiApplicationPlugins())
  {
    if (plug->handleStartup())
    {
      return;
    }
  }

  if (m_applicationSettings.gui)
  {
    auto sqa = safe_cast<SafeQApplication*>(m_app);
    connect(
        sqa, &SafeQApplication::fileOpened, this, [&](const QString& file) {
          m_presenter->documentManager().loadFile(ctx, file);
        });
  }

  // Try to reload if there was a crash
  if (m_applicationSettings.tryToRestore)
  {
    #if defined(SCORE_SPLASH_SCREEN)
    if(m_startScreen && score::OpenDocumentsFile::exists())
    {
      m_startScreen->addLoadCrashedSession();
      connect(
          m_startScreen, &score::StartScreen::loadCrashedSession, this, [&]() {
            m_startScreen->close();
            m_presenter->documentManager().restoreDocuments(ctx);
          });
    }
    #else
    if(score::DocumentBackups::canRestoreDocuments())
    {
      m_presenter->documentManager().restoreDocuments(ctx);
    }
    #endif
  }

  // If nothing was reloaded, open a normal document
  #if !defined(SCORE_SPLASH_SCREEN)
  openNewDocument();
  #endif

#if defined(QT_FEATURE_thread)
#if QT_FEATURE_thread == 1
  QThreadPool::globalInstance()->setMaxThreadCount(2);
#endif
#endif
}

void Application::openNewDocument()
{
  auto& ctx = m_presenter->applicationContext();
  if (m_presenter->documentManager().documents().empty())
  {
    auto& documentKinds = m_presenter->applicationComponents()
                              .interfaces<score::DocumentDelegateList>();
    if (!documentKinds.empty()
        && m_presenter->documentManager().documents().empty())
    {
      m_presenter->documentManager().newDocument(
          ctx,
          Id<score::DocumentModel>{score::random_id_generator::getRandomId()},
          *m_presenter->applicationComponents()
               .interfaces<score::DocumentDelegateList>()
               .begin());
    }
  }
}

int Application::exec()
{
  return m_app->exec();
}

#if defined(SCORE_SPLASH_SCREEN)

W_OBJECT_IMPL(score::StartScreen)
W_OBJECT_IMPL(score::InteractiveLabel)

#endif

