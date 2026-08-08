#include "ApplicationPlugin.hpp"

#include <score/application/ScriptEvaluator.hpp>

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
namespace
{
//! The JS plug-in's answer to "run this here". Registered for the whole
//! process: the session layer needs to run a peer's script without knowing
//! what a QJSEngine is.
struct ConsoleEvaluator final : score::ScriptEvaluator
{
  QJSEngine& engine;
  explicit ConsoleEvaluator(QJSEngine& e)
      : engine{e}
  {
  }

  QString evaluate(const score::DocumentContext&, const QString& code) override
  {
    auto res = engine.evaluate(code);
    if(res.isError())
      return QStringLiteral("ERROR: ") + res.toString();
    return res.isUndefined() ? QString{} : res.toString();
  }
};
}

ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& ctx)
    : score::GUIApplicationPlugin{ctx}
{
  // For the console
  m_consoleEngine.globalObject().setProperty("Score", m_consoleEngine.newQObject(new EditJsContext));
  m_consoleEngine.globalObject().setProperty("Util", m_consoleEngine.newQObject(new JsUtils));
  m_consoleEngine.globalObject().setProperty(
      "System", m_consoleEngine.newQObject(new JsSystem));
  m_consoleEngine.globalObject().setProperty(
      "Library", m_consoleEngine.newQObject(new JsLibrary));
  m_consoleEngine.globalObject().setProperty("Device", m_consoleEngine.newQObject(new DeviceContext{m_consoleEngine}));
  m_consoleEngine.globalObject().setProperty("View", m_consoleEngine.newQObject(new JsViewContext));

  // What a peer's script runs through when this machine is the one with the
  // devices. Owned here, for as long as the engine it uses.
  static ConsoleEvaluator evaluator{m_consoleEngine};
  score::scriptEvaluator() = &evaluator;
  connect(&m_consoleEngine, &QQmlEngine::exit, this, [&] {
    for(auto& doc : score::GUIAppContext().docManager.documents())
      doc->commandStack().markCurrentIndexAsSaved();
    qApp->quit();
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
  this->m_start_script = parser.value(script_opt);
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

  if(!m_start_script.isEmpty())
  {
    // restarted per document: the last one created is the one to script
    if(!m_start_script_timer)
    {
      m_start_script_timer = new QTimer{this};
      m_start_script_timer->setSingleShot(true);
      connect(m_start_script_timer, &QTimer::timeout, this, [this] {
        // --script takes either JavaScript source or the path to a file
        QString source = m_start_script;
        if(QFile f{m_start_script}; f.exists() && f.open(QIODevice::ReadOnly))
          source = QString::fromUtf8(f.readAll());

        m_start_script.clear();
        auto res = m_consoleEngine.evaluate(source);
        if(res.isError())
          qWarning() << "--script:" << res.toString();
      });
    }
    m_start_script_timer->start(100);
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
