#include "ApplicationPlugin.hpp"

#include <JS/DocumentPlugin.hpp>
#include <JS/Qml/DeviceContext.hpp>
#include <JS/Qml/EditContext.hpp>
#include <JS/Qml/Utils.hpp>
#include <JS/Qml/ViewContext.hpp>
#include <Library/LibrarySettings.hpp>
#include <LocalTree/LocalTreeDocumentPlugin.hpp>

#include <core/application/ApplicationInterface.hpp>
#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia/detail/thread.hpp>

#include <ossia-qt/invoke.hpp>
#include <ossia-qt/qml_protocols.hpp>

#include <QCommandLineParser>
#include <QFile>
#include <QFileInfo>

#if __has_include(<QQuickWindow>)
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#endif

#if SCORE_HAS_GPU_JS
#include <Gfx/Settings/Model.hpp>
#endif

#include <ossia/network/context.hpp>

namespace JS
{
// Whether --script was given a program or the path of one. An existing file is
// always a path; anything with JS punctuation in it is a program.
static bool stringIsScript(const QString& input)
{
  if(input.isEmpty())
    return false;

  if(QFileInfo fileInfo{input}; fileInfo.exists() && fileInfo.isFile())
    return false;

  if(input.length() > 4096)
    return true;

  for(QChar ch : input)
  {
    const char16_t c = ch.unicode();
    if(c == '\n' || c == '\r' || c == ';' || c == '{' || c == '}' || c == '('
       || c == ')')
      return true;
  }

  return false;
}

ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& ctx)
    : score::GUIApplicationPlugin{ctx}
{
#if __has_include(<QQuickWindow>)
  // Crisp text in every QML UI: distance-field rendering looks blurry at the small
  // font sizes our panels use, native glyph rendering matches the rest of score.
  QQuickWindow::setTextRenderType(QQuickWindow::NativeTextRendering);
#endif

  // For the console
  m_consoleEngine.globalObject().setProperty("Score", m_consoleEngine.newQObject(new EditJsContext));
  m_consoleEngine.globalObject().setProperty("Util", m_consoleEngine.newQObject(new JsUtils));
  m_consoleEngine.globalObject().setProperty(
      "System", m_consoleEngine.newQObject(new JsSystem));
  m_consoleEngine.globalObject().setProperty(
      "Library", m_consoleEngine.newQObject(new JsLibrary));
  m_consoleEngine.globalObject().setProperty("Device", m_consoleEngine.newQObject(new DeviceContext{m_consoleEngine}));
  m_consoleEngine.globalObject().setProperty("View", m_consoleEngine.newQObject(new JsViewContext));
  connect(&m_consoleEngine, &QQmlEngine::exit, this, [&](int retCode) {
    for(auto& doc : score::GUIAppContext().docManager.documents())
      doc->commandStack().markCurrentIndexAsSaved();
    // quit() is exit(0), which discarded the code Qt.exit() was given: a script
    // could stop the app but never report that it had failed.
    qApp->exit(retCode);
    QTimer::singleShot(
        500, [] { score::GUIApplicationInterface::instance().forceExit(); });
  });
  m_asioContext = std::make_shared<ossia::net::network_context>();
  m_processMessages = true;
  m_consoleEngine.globalObject().setProperty(
      "Protocols",
      m_consoleEngine.newQObject(new ossia::qt::qml_protocols{m_asioContext, this}));
  m_asioThread = std::thread{[this] {
    ossia::set_thread_name("ossia app asio");
    while(m_processMessages)
    {
      m_asioContext->run();
    }
  }};

  // For scripts of processes that run in the ui thread:
  m_scriptProcessUIEngine.globalObject().setProperty(
      "Util", m_scriptProcessUIEngine.newQObject(new JsUtils));
  m_scriptProcessUIEngine.globalObject().setProperty(
      "System", m_scriptProcessUIEngine.newQObject(new JsSystem));
  m_scriptProcessUIEngine.globalObject().setProperty(
      "Library", m_scriptProcessUIEngine.newQObject(new JsLibrary));
  m_scriptProcessUIEngine.globalObject().setProperty(
      "View", m_scriptProcessUIEngine.newQObject(new JsViewContext));

  // Command-line option parsing
  QCommandLineParser parser;

  QCommandLineOption script_opt(
      "script", QCoreApplication::translate("js", "script"), "Script", "");
  parser.addOption(script_opt);

  parser.parse(ctx.applicationSettings.arguments);
  for(const QString& script : parser.values(script_opt))
  {
    if(script.isEmpty())
      continue;

    if(stringIsScript(script))
    {
      this->m_start_scripts.push_back(StartScript{.source = script});
      continue;
    }

    QFile f{script};
    if(!f.open(QIODevice::ReadOnly))
    {
      qCritical() << "--script: cannot open" << script << ":" << f.errorString();
      this->m_start_script_failed = true;
      continue;
    }

    const QFileInfo fi{f};
    StartScript s;
    s.name = script;
    s.file = fi.canonicalFilePath();
    s.dir = fi.canonicalPath();
    // .mjs is what the rest of score calls an ES module (see the library's
    // ModuleLibraryHandler); only those go through importModule().
    s.module = fi.suffix().compare(QStringLiteral("mjs"), Qt::CaseInsensitive) == 0;
    if(!s.module)
      s.source = QString::fromUtf8(f.readAll());

    this->m_start_scripts.push_back(std::move(s));
  }
}

QJSValue ApplicationPlugin::importModule(QQmlEngine& engine, const QString& path)
{
  QJSValue mod = engine.importModule(path);
  if(mod.isError())
    return mod;

  if(auto init = mod.property("initialize"); init.isCallable())
    if(const auto res = init.call(); res.isError())
      return res;

  engine.globalObject().setProperty(QFileInfo{path}.baseName(), mod);
  return mod;
}

void ApplicationPlugin::on_newDocument(score::Document& doc)
{
  score::addDocumentPlugin<DocumentPlugin>(doc);
}

ApplicationPlugin::~ApplicationPlugin()
{
  m_processMessages = false;
  m_asioContext->context.stop();
  m_asioThread.join();
}

void ApplicationPlugin::on_createdDocument(score::Document& doc)
{
  // Local Tree
  LocalTree::DocumentPlugin* lt = doc.context().findPlugin<LocalTree::DocumentPlugin>();
  if(lt)
  {
    auto& root = lt->device().get_root_node();

    auto node = root.create_child("script");
    auto address = node->create_parameter(ossia::val_type::STRING);
    address->set_value(std::string{});
    address->set_access(ossia::access_mode::SET);
    address->add_callback([&](const ossia::value& v) {
      ossia::qt::run_async(
          this, [this, str = QString::fromStdString(ossia::convert<std::string>(v))] {
        auto res = m_consoleEngine.evaluate(str);
        if(res.isError())
        {
          qDebug() << res.toString();
        }
      });
    });
  }

  // Custom data
  if(auto customData = doc.context().findPlugin<DocumentPlugin>(); !customData)
    score::addDocumentPlugin<DocumentPlugin>(doc);

  if(m_start_script_failed)
  {
    qGuiApp->exit(2);
    return;
  }

  if(!m_start_scripts.empty())
  {
    QTimer::singleShot(100, this, [this] {
      for(const StartScript& s : m_start_scripts)
      {
        if(!s.dir.isEmpty())
          m_consoleEngine.addImportPath(s.dir);

        // Report a throwing --script: an unresolvable readFile returns an empty
        // string and eval("") is a no-op, so the process would otherwise exit
        // reporting success and a harness could not tell that from a pass.
        const auto res = s.module ? importModule(m_consoleEngine, s.file)
                                  : m_consoleEngine.evaluate(s.source, s.name);
        if(res.isError())
        {
          qCritical().noquote()
              << "--script:"
              << (s.name.isEmpty() ? QStringLiteral("<inline>") : s.name) << "line"
              << res.property("lineNumber").toInt() << ":" << res.toString();
          qGuiApp->exit(3);
          return;
        }
      }
    });
  }
}
void ApplicationPlugin::afterStartup()
{
  // Dummy engine setup for JS processes
  // eng.importModule(
  //     "/home/jcelerier/Documents/ossia/score/packages/default/Scripts/include/"
  //     "tonal.mjs");
  for(auto& p : this->context.settings<Library::Settings::Model>().getIncludePaths())
  {
    m_scriptProcessUIEngine.addImportPath(p);
    // The console engine runs --script, the console panel and every library
    // .mjs; without this they could not import what a JS process can, which
    // made the same `import` line work in a process and fail in a script.
    m_consoleEngine.addImportPath(p);
  }

#if __has_include(<QQuickWindow>)
  if(QFileInfo f{context.applicationSettings.ui}; f.isFile())
  {
    m_comp = new QQmlComponent{&m_consoleEngine, f.absoluteFilePath(), this};

    if(auto obj = m_comp->create())
    {
      if(auto item = qobject_cast<QQuickItem*>(obj))
      {
        m_window = new QQuickWindow{};
  // QWidget gets these from QWidgetPrivate::adjustFlags; a bare QQuickWindow
  // does not, and on platforms where Qt draws the chrome itself (wasm) that
  // leaves the window with no title bar, close or minimise button.
  m_window->setFlags(
      m_window->flags() | Qt::Window | Qt::WindowTitleHint | Qt::WindowSystemMenuHint
      | Qt::WindowCloseButtonHint | Qt::WindowMinimizeButtonHint
      | Qt::WindowMaximizeButtonHint);
#if defined(__EMSCRIPTEN__)
  // Qt for wasm reports ShowIsFullScreen unconditionally, so QWindow::show()
  // turns into showFullScreen() for every top level: the requested size is
  // discarded and the full-screen state suppresses the frame. Only Qt::Dialog
  // and Qt::Popup opt out (QWasmIntegration::defaultWindowState).
  m_window->setFlags(m_window->flags() | Qt::Dialog);
#endif
        m_window->setWidth(640);
        m_window->setHeight(480);
        item->setParentItem(m_window->contentItem());
#if defined(__EMSCRIPTEN__)
        m_window->showNormal();
#else
        m_window->show();
#endif
        return;
      }
    }
    else
    {
      qDebug() << m_comp->errorString();
      qGuiApp->exit(1);
    }
    delete m_comp;
  }
#endif
}
}
