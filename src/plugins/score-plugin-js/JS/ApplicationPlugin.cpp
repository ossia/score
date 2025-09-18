#include "ApplicationPlugin.hpp"

#include <JS/Qml/DeviceContext.hpp>
#include <JS/Qml/EditContext.hpp>
#include <JS/Qml/Utils.hpp>
#include <Library/LibrarySettings.hpp>
#include <LocalTree/LocalTreeDocumentPlugin.hpp>

#include <core/application/ApplicationInterface.hpp>
#include <core/document/Document.hpp>
#include <core/presenter/DocumentManager.hpp>

#include <ossia/detail/thread.hpp>

#include <ossia-qt/invoke.hpp>
#include <ossia-qt/qml_protocols.hpp>

#if __has_include(<QQuickWindow>)
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#endif

#include <ossia/network/context.hpp>
namespace JS
{
ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& ctx)
    : score::GUIApplicationPlugin{ctx}
{
  m_engine.globalObject().setProperty("Score", m_engine.newQObject(new EditJsContext));
  m_engine.globalObject().setProperty("Util", m_engine.newQObject(new JsUtils));
  m_engine.globalObject().setProperty("Device", m_engine.newQObject(new DeviceContext));
  connect(&m_engine, &QQmlEngine::exit, this, [&] {
    for(auto& doc : score::GUIAppContext().docManager.documents())
      doc->commandStack().markCurrentIndexAsSaved();
    qApp->quit();
    QTimer::singleShot(
        500, [] { score::GUIApplicationInterface::instance().forceExit(); });
  });

  m_asioContext = std::make_shared<ossia::net::network_context>();
  m_processMessages = true;
  m_engine.globalObject().setProperty(
      "Protocols",
      m_engine.newQObject(new ossia::qt::qml_protocols{m_asioContext, this}));
  m_asioThread = std::thread{[this] {
    ossia::set_thread_name("ossia app asio");
    while(m_processMessages)
    {
      m_asioContext->run();
    }
  }};
}

ApplicationPlugin::~ApplicationPlugin()
{
  m_processMessages = false;
  m_asioContext->context.stop();
  m_asioThread.join();
}

void ApplicationPlugin::on_createdDocument(score::Document& doc)
{
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
        auto res = m_engine.evaluate(str);
        if(res.isError())
        {
          qDebug() << res.toString();
        }
      });
    });
  }
}
void ApplicationPlugin::afterStartup()
{
  // Dummy engine setup for JS processes
  // eng.importModule(
  //     "/home/jcelerier/Documents/ossia/score/packages/default/Scripts/include/"
  //     "tonal.mjs");
  for(auto& p :
      score::AppContext().settings<Library::Settings::Model>().getIncludePaths())
  {
    m_dummyEngine.addImportPath(p);
  }

#if __has_include(<QQuickWindow>)
  if(QFileInfo f{context.applicationSettings.ui}; f.isFile())
  {
    m_comp = new QQmlComponent{&m_engine, f.absoluteFilePath(), this};

    if(auto obj = m_comp->create())
    {
      if(auto item = qobject_cast<QQuickItem*>(obj))
      {
        m_window = new QQuickWindow{};
        m_window->setWidth(640);
        m_window->setHeight(480);
        item->setParentItem(m_window->contentItem());
        m_window->show();
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
