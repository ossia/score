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
#include <core/view/QRecentFilesMenu.h>
#if QT_CONFIG(thread)
#include <QThreadPool>
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
namespace score
{

#if defined(SCORE_SPLASH_SCREEN)

    class InteractiveLabel : public QWidget
    {
        W_OBJECT(InteractiveLabel)

      public:
        InteractiveLabel(const QFont& font, const QString& text, const QString url, QWidget* parent = 0);

        void setOpenExternalLink(bool val) {m_openExternalLink = val;}
        void setPixmaps(const QPixmap& pixmap, const QPixmap& pixmapOn);

        void disableInteractivity();
        void setActiveColor(const QColor& c);

        void labelPressed(const QString& file) W_SIGNAL( labelPressed, file)

      protected:
        void paintEvent(QPaintEvent* event) override;
        void enterEvent(QEvent* event)  override;
        void leaveEvent(QEvent* event)  override;
        void mousePressEvent(QMouseEvent* event)  override;

      private:
        QFont m_font;
        QString m_title;

        QString m_url;
        bool m_openExternalLink;

        bool m_drawPixmap;
        QPixmap m_currentPixmap;
        QPixmap m_pixmap;
        QPixmap m_pixmapOn;

        bool m_interactive;
        QColor m_currentColor;
        QColor m_activeColor;
    };

    InteractiveLabel::InteractiveLabel(const QFont& font, const QString& title, const QString url, QWidget* parent)
      : QWidget{parent}, m_font(font), m_title(title), m_url(url), m_openExternalLink(false), m_drawPixmap(false), m_interactive(true)
    {
      auto& skin = score::Skin::instance();
      m_currentColor = QColor{"#f0f0f0"};
      m_activeColor = QColor{"#03C3DD"};
      setCursor(skin.CursorPointingHand);
      setFixedSize(200,34);
    }

    void InteractiveLabel::setPixmaps(const QPixmap& pixmap, const QPixmap& pixmapOn)
    {
      m_drawPixmap = true;
      m_pixmap = pixmap;
      m_pixmapOn = pixmapOn;
      m_currentPixmap = m_pixmap;
    }

    void InteractiveLabel::disableInteractivity()
    {
      m_interactive = false;
      setCursor(Qt::ArrowCursor);
    }

    void InteractiveLabel::setActiveColor(const QColor& c)
    {
      m_activeColor = c;
    }

    void InteractiveLabel::paintEvent(QPaintEvent* event)
    {
      QPainter painter(this);

      painter.setRenderHint(QPainter::Antialiasing, true);
      painter.setRenderHint(QPainter::TextAntialiasing, true);
      painter.setPen(QPen{m_currentColor});

      QRectF textRect = rect();
      if(m_drawPixmap)
      {
        int size = m_currentPixmap.width();
        painter.drawPixmap(0,0,m_currentPixmap);
        textRect.setX( textRect.x() + size / qApp->devicePixelRatio() + 6);
      }
      painter.setFont(m_font);
      painter.drawText(textRect,m_title);
    }

    void InteractiveLabel::enterEvent(QEvent* event)
    {
      if(!m_interactive)
        return;

      m_currentColor = m_activeColor;
      m_font.setUnderline(true);
      m_currentPixmap = m_pixmapOn;

      repaint();
    }
    void InteractiveLabel::leaveEvent(QEvent* event)
    {
      if(!m_interactive)
        return;

       m_currentColor = QColor{"#f0f0f0"};
       m_font.setUnderline(false);
       m_currentPixmap = m_pixmap;

       repaint();
    }
    void InteractiveLabel::mousePressEvent(QMouseEvent* event)
    {
      if(m_openExternalLink)
      {
        QDesktopServices::openUrl(QUrl(m_url));
      }
      else
      {
        labelPressed(m_url);
      }
    }

    class StartScreen : public QWidget
    {
      W_OBJECT(StartScreen)
      public:
        StartScreen(const QPointer<QRecentFilesMenu>& recentFiles, QWidget* parent = 0);

        void openNewDocument() W_SIGNAL( openNewDocument)
        void openFile(const QString& file) W_SIGNAL( openFile, file)
        void openFileDialog() W_SIGNAL( openFileDialog )
        void loadCrashedSession() W_SIGNAL( loadCrashedSession )

        void addLoadCrashedSession();

      protected:
        void paintEvent(QPaintEvent* event) override;
        void keyPressEvent(QKeyEvent* event) override;

      private:
        QPixmap m_background;
        InteractiveLabel* m_crashLabel;
    };
    struct StartScreenLink
    {
        QString name;
        QString url;
        QString pixmap;
        QString pixmapOn;
        StartScreenLink(const QString& n, const QString& u, const QString& p, const QString& pOn):name(n), url(u), pixmap(p), pixmapOn(pOn){}
    };

    StartScreen::StartScreen(const QPointer<QRecentFilesMenu>& recentFiles, QWidget* parent)
      :QWidget(parent)
    {
      this->setEnabled(true);
      setWindowFlags( Qt::Dialog | Qt::FramelessWindowHint );//| Qt::WindowStaysOnTopHint);
      setWindowModality(Qt::ApplicationModal);

      m_background = score::get_pixmap(":/startscreen/startscreensplash.png");


      QFont f("Ubuntu", 14, QFont::Light);
      f.setHintingPreference(QFont::HintingPreference::PreferFullHinting);
      f.setStyleStrategy(QFont::PreferAntialias);

      QFont titleFont("Montserrat", 14, QFont::DemiBold);
      titleFont.setHintingPreference(QFont::HintingPreference::PreferFullHinting);
      titleFont.setStyleStrategy(QFont::PreferAntialias);

      if (QPainter painter; painter.begin(&m_background))
      {
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);
        painter.setPen(QPen(QColor("#0092CF")));

        painter.setFont(f);
        //painter.drawText(QPointF(250, 170), QCoreApplication::applicationVersion());
        painter.drawText(QPointF(381, 195), QCoreApplication::applicationVersion());
        painter.end();
      }
      setFixedSize(m_background.size() / qApp->devicePixelRatio());

      float label_y = 285;
      { // Create new
        InteractiveLabel* label = new InteractiveLabel{titleFont, qApp->tr("New"), "", this};
        label->setPixmaps(score::get_pixmap(":/icons/new_file_off.png"),score::get_pixmap(":/icons/new_file_on.png"));
        connect(label, &score::InteractiveLabel::labelPressed,
                this, &score::StartScreen::openNewDocument);
        label->move(100,label_y);
        label_y += 35;
      }
      { // Load file
        InteractiveLabel* label = new InteractiveLabel{titleFont, qApp->tr("Load"), "", this};
        label->setPixmaps(score::get_pixmap(":/icons/load_off.png"),score::get_pixmap(":/icons/load_on.png"));
        connect(label, &score::InteractiveLabel::labelPressed,
                this, &score::StartScreen::openFileDialog);
        label->move(100,label_y);
        label_y += 35;
      }
      { // Load Examples
        auto paths = QStandardPaths::standardLocations(
            QStandardPaths::DocumentsLocation);
        InteractiveLabel* label = new InteractiveLabel{titleFont, qApp->tr("Examples"), "file://" + paths[0] + "/ossia score library/", this};
        label->setPixmaps(score::get_pixmap(":/icons/load_examples_off.png"),score::get_pixmap(":/icons/load_examples_on.png"));
        label->setOpenExternalLink(true);
        label->move(100,label_y);
        label_y += 50;
      }
      label_y = 285;
      { // recent files
        InteractiveLabel* label = new InteractiveLabel{titleFont, qApp->tr("Recent files"), "", this};
        label->setPixmaps(score::get_pixmap(":/icons/recent_files.png"),score::get_pixmap(":/icons/recent_files.png"));
        label->disableInteractivity();
        label->move(280,label_y);
        label_y += 30;
      }
      f.setPointSize(12);
      for(const auto& action: recentFiles->actions())
      {
        InteractiveLabel* fileLabel = new InteractiveLabel{f, action->text(), action->data().toString(), this};
        connect(fileLabel, &score::InteractiveLabel::labelPressed,
                this, &score::StartScreen::openFile);

        fileLabel->move(310,label_y);

        label_y += 25;
      }

      std::array<score::StartScreenLink, 4> menus = {{
        {qApp->tr("Tutorials"),"https://www.youtube.com/playlist?list=PLIHLSiZpIa6aRQT5v6RInuyCR3qWmMEgV", ":/icons/tutorials_off.png", ":/icons/tutorials_on.png"},
        {qApp->tr("Contribute"),"https://opencollective.com/ossia/contribute", ":/icons/contribute_off.png", ":/icons/contribute_on.png"},
        {qApp->tr("Forum"),"http://forum.ossia.io/", ":/icons/forum_off.png", ":/icons/forum_on.png"},
        {qApp->tr("Chat"),"https://gitter.im/ossia/score", ":/icons/chat_off.png", ":/icons/chat_on.png"}
      }};
      label_y = 285;
      for(const auto& m: menus)
      {
        InteractiveLabel* menu_url = new InteractiveLabel{titleFont, m.name, m.url, this};
        menu_url->setOpenExternalLink(true);
        menu_url->setPixmaps(score::get_pixmap(m.pixmap), score::get_pixmap(m.pixmapOn));
        menu_url->move(530,label_y);
        label_y+=40;
      }

      m_crashLabel = new InteractiveLabel{titleFont, qApp->tr("Reload your previously crashed work ?"), "", this};
      m_crashLabel->setPixmaps(score::get_pixmap(":/icons/reload_crash_off.png"),score::get_pixmap(":/icons/reload_crash_on.png"));
      m_crashLabel->move(150,460);
      m_crashLabel->setFixedWidth(600);
      m_crashLabel->setActiveColor(QColor{"#f6a019"});
      m_crashLabel->setDisabled(true);
      m_crashLabel->hide();
      connect(m_crashLabel, &score::InteractiveLabel::labelPressed,
              this, &score::StartScreen::loadCrashedSession);
    }

    void StartScreen::addLoadCrashedSession()
    {
      m_crashLabel->show();
      m_crashLabel->setEnabled(true);
      update();
    }

    void StartScreen::paintEvent(QPaintEvent* event)
    {
      QPainter painter(this);
      painter.drawPixmap(0,0, m_background);
    }

    void StartScreen::keyPressEvent(QKeyEvent* event)
    {
      if (event->key() == Qt::Key_Escape)
      {
          this->close();
          return QWidget::keyPressEvent(event);
      }
    }

#else
class StartScreen : public QWidget
{

};
#endif

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

  m_app = createApplication(m_applicationSettings, argc, argv);
}

Application::Application(
    const score::ApplicationSettings& appSettings, int& argc, char** argv)
    : QObject{nullptr}, m_applicationSettings(appSettings)
{
  m_instance = this;
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
    if(platform.contains("wayland"))
    {
      m_applicationSettings.opengl = false;
    }
    else
    {
      QOpenGLContext ctx;
      m_applicationSettings.opengl = ctx.create();
    }
  }
#if defined(SCORE_STATIC_PLUGINS)
  score_init_static_plugins();
#endif

  std::vector<spdlog::sink_ptr> v;
  try {
    v.push_back(std::make_shared<spdlog::sinks::stderr_sink_mt>());
  } catch (...) { }
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

#if defined(SCORE_SPLASH_SCREEN)
  if (m_applicationSettings.gui)
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

  if (auto sqa = dynamic_cast<SafeQApplication*>(m_app))
  {
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

#if QT_CONFIG(thread)
  QThreadPool::globalInstance()->setMaxThreadCount(2);
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

