// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "Application.hpp"

#include <score/application/ApplicationComponents.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Validity/ValidityChecker.hpp>
#include <score/model/Identifier.hpp>
#include <score/model/path/ObjectIdentifier.hpp>
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/documentdelegate/DocumentDelegateFactory.hpp>
#include <score/selection/Selection.hpp>
#include <score/tools/IdentifierGeneration.hpp>
#include <score/tools/std/Optional.hpp>
#include <score/widgets/Pixmap.hpp>
#include <core/view/StyleLoader.hpp>

#include <core/application/ApplicationRegistrar.hpp>
#include <core/application/ApplicationSettings.hpp>
#include <core/application/SafeQApplication.hpp>
#include <core/document/DocumentBackups.hpp>
#include <core/document/DocumentModel.hpp>
#include <core/presenter/DocumentManager.hpp>
#include <core/presenter/Presenter.hpp>
#include <core/view/Window.hpp>

#include <ossia-qt/qt_logger.hpp>
#include <ossia/context.hpp>
#include <ossia/detail/logger.hpp>

#include <QByteArray>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFont>
#include <QFontDatabase>
#include <QPainter>
#include <QPixmap>
#include <QSplashScreen>
#include <QString>
#include <QStringList>
#include <qnamespace.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_sinks.h>

#include <algorithm>
#include <vector>

#include <QOpenGLContext>
#if __has_include(<QQuickStyle>)
#include <QQuickStyle>
#endif

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

#include <phantom/phantomstyle.h>
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
  QFontDatabase::addApplicationFont(
      ":/Catamaran-Regular.ttf"); // Catamaran Regular
  QFontDatabase::addApplicationFont(":/Montserrat-Regular.ttf"); // Montserrat

  static score::StyleLoader style;
  style.on_styleChanged();
  m_app.setStyle(new PhantomStyle);

  auto pal = qApp->palette();
  pal.setBrush(QPalette::Window, QColor("#1A2024"));
  pal.setBrush(QPalette::Base, QColor("#12171A"));          // lineedit bg
  pal.setBrush(QPalette::Button, QColor("#12171A"));        // lineedit bg
  pal.setBrush(QPalette::AlternateBase, QColor("#1f2a30")); // alternate bg
  pal.setBrush(QPalette::Highlight, QColor("#3d8ec9"));     // tableview bg
  pal.setBrush(QPalette::WindowText, QColor("silver"));     // color
  pal.setBrush(QPalette::Text, QColor("silver"));           // color
  pal.setBrush(QPalette::ButtonText, QColor("silver"));     // color
  pal.setBrush(QPalette::Light, QColor("#666666"));
  pal.setBrush(QPalette::Midlight, QColor("#666666"));
  pal.setBrush(QPalette::Mid, QColor("#666666"));
  pal.setBrush(QPalette::Dark, QColor("#666666"));
  pal.setBrush(QPalette::Shadow, QColor("#666666"));

  // note : on win32 this does not seem to have any impact
  // check whether it is used somewhere...
#if defined(_WIN32)
  constexpr const int defaultFontSize = 7;
#else
  constexpr const int defaultFontSize = 10;
#endif
  QFont f("Ubuntu", defaultFontSize);
  qApp->setFont(f);

  qApp->setPalette(pal);

#if __has_include(<QQuickStyle>)
  QQuickStyle::setStyle(":/desktopqqc2style/Desktop");
#endif
}

} // namespace score

Application::Application(int& argc, char** argv) : QObject{nullptr}
{
  m_instance = this;

  QStringList l;
  for (int i = 0; i < argc; i++)
    l.append(QString::fromUtf8(argv[i]));
  m_applicationSettings.parse(l, argc, argv);

  if (m_applicationSettings.gui)
    m_app = new SafeQApplication{argc, argv};
  else
    m_app = new QCoreApplication{argc, argv};
}

Application::Application(
    const score::ApplicationSettings& appSettings, int& argc, char** argv)
    : QObject{nullptr}, m_applicationSettings(appSettings)
{
  m_instance = this;
  if (m_applicationSettings.gui)
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

const score::ApplicationComponents& Application::components() const
{
  return m_presenter->applicationComponents();
}

#if !defined(SCORE_DEBUG) && !defined(__EMSCRIPTEN__)
#define SCORE_SPLASH_SCREEN 1
#endif

#if defined(SCORE_SPLASH_SCREEN)
static QPixmap writeVersionName()
{
  QImage pixmap = score::get_image(":/splash.png");

  QPainter painter;
  if (!painter.begin(&pixmap))
  {
    return QPixmap::fromImage(pixmap);
  }

  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.setRenderHint(QPainter::TextAntialiasing, true);
  painter.setRenderHint(QPainter::HighQualityAntialiasing, true);
  painter.setPen(QPen(QColor("#0092CF")));
  QFont f("Ubuntu", 14, QFont::Light);
  f.setHintingPreference(QFont::HintingPreference::PreferFullHinting);
  f.setStyleStrategy(QFont::PreferAntialias);
  painter.setFont(f);
  painter.drawText(QPointF(250, 170), QCoreApplication::applicationVersion());
  painter.end();

  return QPixmap::fromImage(pixmap);
}
#endif

void Application::init()
{
  if(m_applicationSettings.gui && m_applicationSettings.opengl)
  {
    QOpenGLContext ctx;

    m_applicationSettings.opengl = ctx.create();
  }
#if defined(SCORE_STATIC_PLUGINS)
  score_init_static_plugins();
#endif

  std::vector<spdlog::sink_ptr> v{std::make_shared<spdlog::sinks::stderr_sink_mt>(),
                                  std::make_shared<ossia::qt::log_sink>()};

  ossia::context context{v};
  ossia::logger().set_level(spdlog::level::debug);

  score::setQApplicationMetadata();
#if defined(SCORE_SPLASH_SCREEN)
  QSplashScreen* splash{};
  if (m_applicationSettings.gui)
  {
    splash = new QSplashScreen{writeVersionName(), Qt::FramelessWindowHint};
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
  auto path = qgetenv("PATH");
  path += ";" + QCoreApplication::applicationDirPath();
  path += ";" + QCoreApplication::applicationDirPath() + "/plugins";
  qputenv("PATH", path);
  SetDllDirectoryW((wchar_t*)QCoreApplication::applicationDirPath().utf16());
  SetDllDirectoryW(
      (wchar_t*)(QCoreApplication::applicationDirPath() + "/plugins").utf16());
#endif

  // MVP
  if (m_applicationSettings.gui)
  {
    score::setQApplicationSettings(*qApp);
    m_settings.setupView();
    m_projectSettings.setupView();
    m_view = new score::View{this};
  }

  m_presenter = new score::Presenter{m_applicationSettings, m_settings,
                                     m_projectSettings, m_view, this};

#if defined(SCORE_SPLASH_SCREEN)
    if (splash)
    {
      splash->finish(m_view);
      splash->deleteLater();
    }
#endif
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

  if (auto sqa = dynamic_cast<SafeQApplication*>(m_app))
  {
    connect(
        sqa, &SafeQApplication::fileOpened, this, [&](const QString& file) {
          m_presenter->documentManager().loadFile(ctx, file);
        });
  }

  // Try to reload if there was a crash
  if (m_applicationSettings.tryToRestore
      && score::DocumentBackups::canRestoreDocuments())
  {
    m_presenter->documentManager().restoreDocuments(ctx);
  }

  // If nothing was reloaded, open a normal document
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
