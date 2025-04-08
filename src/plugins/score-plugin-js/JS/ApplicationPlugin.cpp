#include "ApplicationPlugin.hpp"

#include <JS/Qml/DeviceContext.hpp>
#include <JS/Qml/EditContext.hpp>
#include <JS/Qml/Utils.hpp>
#include <Library/LibrarySettings.hpp>

#if __has_include(<QQuickWindow>)
#include <QGuiApplication>
#include <QQuickItem>
#include <QQuickWindow>
#endif

namespace JS
{
ApplicationPlugin::ApplicationPlugin(const score::GUIApplicationContext& ctx)
    : score::GUIApplicationPlugin{ctx}
{
  m_engine.globalObject().setProperty("Score", m_engine.newQObject(new EditJsContext));
  m_engine.globalObject().setProperty("Util", m_engine.newQObject(new JsUtils));
  m_engine.globalObject().setProperty("Device", m_engine.newQObject(new DeviceContext));
}

ApplicationPlugin::~ApplicationPlugin() { }

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
