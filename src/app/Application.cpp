// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Application.hpp"

#include <score/application/ApplicationServices.hpp>
#include <score/command/Validity/ValidityChecker.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/Skin.hpp>
#include <score/model/path/ObjectIdentifier.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/application/NetworkSessionInterface.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/plugins/settingsdelegate/SettingsDelegateModel.hpp>
#include <score/selection/Selection.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/widgets/Pixmap.hpp>

#include <core/application/ApplicationRegistrar.hpp>
#include <core/application/OpenDocumentsFile.hpp>
#include <core/application/SafeQApplication.hpp>
#include <core/document/DocumentBackups.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/view/Window.hpp>

#include <ossia/context.hpp>

#include <ossia-qt/qt_logger.hpp>

#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QLabel>
#include <QOpenGLContext>
#include <QPainter>
#include <QPushButton>
#include <QResource>
#include <QStandardPaths>
#include <QStyleHints>
#include <QUrl>
#include <qconfig.h>
#include <qobjectdefs.h>

#if defined(QT_FEATURE_thread)
#if QT_FEATURE_thread == 1
#include <QThreadPool>
#endif
#endif

#include <score/gfx/Vulkan.hpp>
#if QT_HAS_VULKAN && __has_include(<QVulkanInstance>)
#include <QVulkanInstance>
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
int qInitResources_qtconf();
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
public:
  void dismiss() { }
};
}
#endif

static void loadResources()
{
// Note: Q_INIT_RESOURCE must be invoked outside of any namespace
#if defined(SCORE_STATIC_PLUGINS)
  Q_INIT_RESOURCE(score);
  Q_INIT_RESOURCE(qtconf);
  Q_INIT_RESOURCE(qcodeeditor_resources);
#if defined(__APPLE__)
  Q_INIT_RESOURCE(fonts_macos);
#else
  Q_INIT_RESOURCE(fonts);
#endif
#endif

  if(QString file = QCoreApplication::applicationDirPath() + "/resources.rcc";
     QFile::exists(file))
  {
    QResource::registerResource(file);
  }
}

namespace score
{
class DocumentModel;

static void loadApplicationResources()
{
  loadResources();

  // Register fonts
  {
    QDirIterator it(":/fonts", QDirIterator::Subdirectories);
    while(it.hasNext())
    {
      auto font = it.next();
      if(font.endsWith("ttf", Qt::CaseInsensitive)
         || font.endsWith("bdf", Qt::CaseInsensitive)
         || font.endsWith("otf", Qt::CaseInsensitive))
      {
        QFontDatabase::addApplicationFont(font);
      }
    }
  }
  // Read straight from QSettings rather than through
  // Scenario::Settings::Model: the application font has to be in place before
  // the plug-ins that own that model are loaded. Same keys, so the settings
  // page stays authoritative -- it just needs a restart to take effect.
}

//! Must run after QApplication::setStyle(), which resets the widget font hash.
static void setupApplicationFont()
{
  QFont f("Ubuntu");
  f.setPixelSize(score::uiFontSize());
  f.setHintingPreference(score::uiFontHinting());
  f.setStyleStrategy(score::uiFontStyleStrategy());
  qGuiApp->setFont(f);

  // The platform theme seeds per-class fonts which override the application
  // font; macOS provides most of this list, so set them explicitly.
  for(const char* widgetClass :
      {"QMenu", "QMenuBar", "QMenuItem", "QMessageBox", "QLabel", "QTipLabel",
       "QTitleBar", "QStatusBar", "QMdiSubWindowTitleBar", "QDockWidgetTitle",
       "QPushButton", "QCheckBox", "QRadioButton", "QToolButton",
       "QAbstractItemView", "QListView", "QHeaderView", "QListBox",
       "QComboMenuItem", "QComboLineEdit", "QSmallFont", "QMiniFont"})
  {
    QApplication::setFont(f, widgetClass);
  }
}

static void setQApplicationSettings(QApplication& m_app)
{
  // For release builds against a debug Qt, we build qt without any style plug-in.
  // Sadly Qt asserts so wh have to simulate the loading of a plugin (see above).
  // For older Qts we won't be debugging anyways and will be linking against distro Qt versions so we just set the style
  // manually
  m_app.setStyle(new PhantomStyle);

  auto pal = qApp->palette();
  pal.setBrush(QPalette::Window, QColor("#222222"));        //#1A2024"));
  pal.setBrush(QPalette::Base, QColor("#161514"));          //12171A"));
  pal.setBrush(QPalette::AlternateBase, QColor("#1e1d1c")); //1f2a30")); // alternate bg
  pal.setBrush(QPalette::Highlight, QColor{"#9062400a"});
  pal.setBrush(QPalette::HighlightedText, QColor("#FDFDFD"));
  pal.setBrush(QPalette::WindowText, QColor("silver"));
  pal.setBrush(QPalette::Text, QColor("#d0d0d0"));

  pal.setBrush(QPalette::Button, QColor("#1d1c1a"));
  pal.setBrush(QPalette::ButtonText, QColor("#f0f0f0"));
  pal.setBrush(QPalette::PlaceholderText, QColor("#80d0d0d0"));
  pal.setBrush(QPalette::ToolTipBase, QColor("#161514"));
  pal.setBrush(QPalette::ToolTipText, QColor("silver"));

  pal.setBrush(QPalette::Midlight, QColor{"#62400a"});
  pal.setBrush(QPalette::Light, QColor{"#c58014"});
  pal.setBrush(QPalette::Mid, QColor("#252930"));

  //  pal.setBrush(QPalette::Dark, QColor("#808080"));
  // pal.setBrush(QPalette::Shadow, QColor("#666666"));

  qGuiApp->setPalette(pal);
}

} // namespace score

namespace
{
bool runningUnderAnUISession() noexcept
{
  const auto platform = qgetenv("QT_QPA_PLATFORM");
  if(platform == "minimal")
  {
    return false;
  }

  // Win32 and macOS always have a graphical session
#if defined(WIN32) || defined(__APPLE__)
  return true;
#else
  if(!qEnvironmentVariableIsEmpty("DISPLAY"))
    return true;
  if(!qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))
    return true;
  if(qgetenv("XDG_SESSION_TYPE") != "tty")
    return true;
  if(platform.contains("gl") || platform.contains("vkkhr")
     || platform.contains("linuxfb") || platform.contains("vnc")
     || platform.contains("offscreen"))
    return true;
  return false;
#endif
}

QCoreApplication*
createApplication(const score::ApplicationSettings& set, int& argc, char** argv)
{
  if(set.gui || !set.ui.isEmpty())
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

Application::Application(int& argc, char** argv)
    : QObject{nullptr}
{
  m_instance = this;

  QStringList l;
  for(int i = 0; i < argc; i++)
    l.append(QString::fromUtf8(argv[i]));
  appSettings.parse(l, argc, argv);

  score::setQApplicationMetadata();

  if(!qEnvironmentVariableIsSet("QT_SCALE_FACTOR"))
  {
    QSettings s;
    if(s.contains("Skin/Zoom"))
    {
      double zoom = s.value("Skin/Zoom").toDouble();
      if(zoom != 1.)
      {
        zoom = qBound(1.0, zoom, 10.);
        qputenv("QT_SCALE_FACTOR", QString("%1").arg(zoom).toLatin1());
        qputenv("QT_SCALE_FACTOR_ROUNDING_POLICY", "PassThrough");
      }
    }
  }

  m_app = createApplication(appSettings, argc, argv);

#if defined(QT_FEATURE_thread)
#if QT_FEATURE_thread == 1
  this->thread()->setPriority(QThread::Priority::TimeCriticalPriority);
  QThreadPool::globalInstance()->setMaxThreadCount(2);
  QThreadPool::globalInstance()->setThreadPriority(QThread::Priority::HighPriority);
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
  QThreadPool::globalInstance()->setServiceLevel(QThread::QualityOfService::High);
#endif
#endif
#endif
}

Application::Application(
    const score::ApplicationSettings& appSettings, int& argc, char** argv)
    : QObject{nullptr}
    , appSettings(appSettings)
{
  m_instance = this;
  score::setQApplicationMetadata();

  if(!qEnvironmentVariableIsSet("QT_SCALE_FACTOR"))
  {
    QSettings s;
    if(s.contains("Skin/Zoom"))
    {
      double zoom = s.value("Skin/Zoom").toDouble();
      if(zoom != 1.)
      {
        zoom = qBound(1.0, zoom, 10.);
        qputenv("QT_SCALE_FACTOR", QString("%1").arg(zoom).toLatin1());
        qputenv("QT_SCALE_FACTOR_ROUNDING_POLICY", "PassThrough");
      }
    }
  }

  m_app = createApplication(appSettings, argc, argv);
}

Application::~Application()
{
  this->setParent(nullptr);
  m_settings.teardownView();
  // FIXME projectSettings?
  delete m_view;
  delete m_presenter;
  delete m_startScreen;

  score::DocumentBackups::clear();
  QCoreApplication::processEvents();

  auto& svc = score::AppServices();
  svc.filewatch.reset();
  svc.taskpool.reset();
  svc.threadpool.reset();

#if QT_HAS_VULKAN
  if(auto vk = score::gfx::staticVulkanInstance(false))
  {
    delete vk;
  }

#endif
  for(auto& settings : m_settings.settings())
    settings->setParent(nullptr);
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
  if(appSettings.gui && appSettings.opengl)
  {
    auto platform = QGuiApplication::platformName();
    if(platform.contains(QStringLiteral("wayland")))
    {
      appSettings.opengl = false;
    }
    else
    {
#if !((defined(__arm__) || defined(__aarch64__)))
      QOpenGLContext ctx;
      appSettings.opengl = ctx.create() && ctx.format().majorVersion() > 1;
#else
      appSettings.opengl = true;
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
  try
  {
    v.push_back(std::make_shared<ossia::qt::log_sink>());
  }
  catch(...)
  {
  }

  ossia::context context{std::move(v)};
  ossia::logger().set_level(spdlog::level::debug);

  this->setObjectName("Application");
  this->setParent(m_app);
#if !defined(__EMSCRIPTEN__)
#if QT_CONFIG(library)
  m_app->addLibraryPath(m_app->applicationDirPath() + "/plugins");
#endif
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

  if(appSettings.ui.isEmpty())
  {
    if(!qEnvironmentVariableIsSet("QT_QUICK_CONTROLS_STYLE"))
    {
      qputenv("QT_QUICK_CONTROLS_STYLE", "Basic");
#if QT_VERSION >= QT_VERSION_CHECK(6, 8, 0)
      if(appSettings.gui)
        qGuiApp->styleHints()->setColorScheme(Qt::ColorScheme::Dark);
#endif
    }
  }

  // MVP
  if(appSettings.gui)
  {
    score::loadApplicationResources();
    score::setQApplicationSettings(*qApp);
    score::setupApplicationFont();
    m_settings.setupView();
    //m_projectSettings.setupView();
    m_view = new score::View{this};
  }
  else if(!appSettings.ui.isEmpty())
  {
    score::loadApplicationResources();
  }

  m_presenter
      = new score::Presenter{appSettings, m_settings, m_projectSettings, m_view, this};
  // Plugins
  GUIApplicationInterface::loadPluginData(m_settings, *m_presenter);

  // View
  if(appSettings.gui)
  {
    bool show_fullscreen = false;
#if defined(__EMSCRIPTEN__)
    show_fullscreen = true;
#else
    show_fullscreen = (qGuiApp->platformName() == "vnc");
#endif
    if(show_fullscreen)
    {
      m_view->setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
      m_view->showFullScreen();
    }
    else
    {
      m_view->show();
    }
  }

#if defined(__APPLE__)
  {
    if(appSettings.gui)
    {
      auto sqa = safe_cast<SafeQApplication*>(m_app);
      if(auto file = sqa->fileToOpen; QFile::exists(file))
      {
        appSettings.loadList.push_back(file);
      }
    }
  }
#endif

#if defined(SCORE_SPLASH_SCREEN)
  // --script runs on document creation: with nothing to open, skip the start screen.
  if(appSettings.gui && !appSettings.forceRestore && appSettings.loadList.empty()
     && !appSettings.hasScript)
  {
    m_startScreen = new score::StartScreen{this->context().docManager.recentFiles()};
    m_startScreen->show();

    auto& ctx = m_presenter->applicationContext();
    connect(m_startScreen, &score::StartScreen::openNewDocument, this, [&]() {
      m_startScreen->close();
      openNewDocument();
    });
    // The start screen steps aside while a document is being chosen or created;
    // if nothing comes out of it (cancelled dialog, missing file...) it comes back
    // instead of leaving an empty window behind.
    auto settle = [this](score::Document* doc) {
      if(doc)
        m_startScreen->close();
      else
        m_startScreen->reopen();
    };
    connect(
        m_startScreen, &score::StartScreen::openFile, this,
        [this, &ctx, settle](const QString& file) {
      m_startScreen->hide();
      settle(m_presenter->documentManager().loadFile(ctx, file));
    });
    connect(
        m_startScreen, &score::StartScreen::openFileDialog, this, [this, &ctx, settle] {
      m_startScreen->hide();
      settle(m_presenter->documentManager().loadFile(ctx));
    });
    connect(
        m_startScreen, &score::StartScreen::openTemplate, this,
        [this, &ctx, settle](const QString& file) {
      m_startScreen->hide();
      settle(m_presenter->documentManager().newDocumentFromTemplate(ctx, file));
    });
    connect(m_startScreen, &score::StartScreen::exitApp, this, [&]() { qApp->quit(); });

    if(auto net = score::findNetworkSessionInterface(ctx))
    {
      m_startScreen->addJoinSession();
      connect(m_startScreen, &score::StartScreen::joinSession, this, [this, net] {
        m_startScreen->hide();
        net->joinSession(m_view, [this](bool joined) {
          if(joined)
            m_startScreen->close();
          else if(m_presenter->documentManager().documents().empty())
            m_startScreen->reopen();
          else
            m_startScreen->close();
        });
      });
    }
  }
#endif

  if(appSettings.gui)
  {
    m_view->sizeChanged(m_view->size());
    m_view->ready();

    auto sqa = safe_cast<SafeQApplication*>(m_app);
    connect(sqa, &SafeQApplication::fileOpened, this, [&](const QString& file) {
      auto& ctx = m_presenter->applicationContext();
      m_presenter->documentManager().loadFile(ctx, file);
    });
  }

  QTimer::singleShot(10, [&] {
    initDocuments();

#if defined(QT_FEATURE_thread)
#if QT_FEATURE_thread == 1
    this->thread()->setPriority(QThread::Priority::TimeCriticalPriority);
    QThreadPool::globalInstance()->setMaxThreadCount(2);
    QThreadPool::globalInstance()->setThreadPriority(QThread::Priority::HighPriority);
#if QT_VERSION >= QT_VERSION_CHECK(6, 9, 0)
    QThreadPool::globalInstance()->setServiceLevel(QThread::QualityOfService::High);
#endif
#endif
#endif

    auto& ctx = m_presenter->applicationContext();
    // The plug-ins have the ability to override the boot process.
    for(auto plug : ctx.guiApplicationPlugins())
    {
      plug->afterStartup();
    }
  });
}

void Application::initDocuments()
{
  auto& ctx = m_presenter->applicationContext();
  // The plug-ins have the ability to override the boot process.
  for(auto plug : ctx.guiApplicationPlugins())
  {
    if(plug->handleLoading())
    {
      // e.g. --network-join: the plug-in provides the document
      if(m_startScreen)
        m_startScreen->dismiss();
      return;
    }
  }

  if(!appSettings.loadList.empty())
  {
    for(const auto& doc : appSettings.loadList)
      m_presenter->documentManager().loadFile(ctx, doc);
  }

  // Try to reload if there was a crash
  if(appSettings.forceRestore)
  {
    if(score::OpenDocumentsFile::exists())
    {
      m_presenter->documentManager().restoreDocuments(ctx);
    }
  }
  else if(appSettings.tryToRestore)
  {
#if defined(SCORE_SPLASH_SCREEN)
    if(m_startScreen && score::OpenDocumentsFile::exists())
    {
      m_startScreen->addLoadCrashedSession();
      connect(m_startScreen, &score::StartScreen::loadCrashedSession, this, [&]() {
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
  if(!appSettings.ui.isEmpty())
  {
    // Custom UI mode always expects a new document
    openNewDocument();
  }
  else
  {
    if(!m_startScreen)
      openNewDocument();
  }
}

void Application::openNewDocument()
{
  auto& ctx = m_presenter->applicationContext();
  if(m_presenter->documentManager().documents().empty())
  {
    auto& documentKinds
        = m_presenter->applicationComponents().interfaces<score::DocumentDelegateList>();
    if(!documentKinds.empty() && m_presenter->documentManager().documents().empty())
    {
      m_presenter->documentManager().newDocument(
          ctx, Id<score::DocumentModel>{score::random_id_generator::getRandomId()},
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
