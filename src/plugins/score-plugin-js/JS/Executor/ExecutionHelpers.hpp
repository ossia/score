#pragma once
#include <JS/Qml/QmlObjects.hpp>
#include <JS/Commands/EditScript.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>
#include <Library/LibrarySettings.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/application/GUIApplicationContext.hpp>

#include <ossia/detail/logger.hpp>
#include <ossia-qt/invoke.hpp>
#include <ossia-qt/qml_engine_functions.hpp>
#include <JS/ConsolePanel.hpp>

#include <QDir>
#include <QFile>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QUrl>


namespace JS
{

inline void connectStateCommit(Script* script, ProcessModel* model)
{
  if(!model)
    return;
  QObject::connect(
      script, &Script::commitState, model,
      [context = QPointer{model}](const QString& key, const QJSValue& value) {
        auto converted = ossia::qt::value_from_js(value);
        QMetaObject::invokeMethod(
            qApp, [context, key, value = std::move(converted)] {
              if(!context)
                return;
              const auto& state = context->state();
              if(auto it = state.find(key); it != state.end() && it->second == value)
                return;
              CommandDispatcher<> dispatcher{
                  score::IDocument::documentContext(*context).commandStack};
              dispatcher.submit<UpdateStateElement>(*context, key, value);
            },
            Qt::QueuedConnection);
      },
      Qt::DirectConnection);
}


inline void loadJSObjectFromString(
    const QString& rootPath, const QByteArray& str, QQmlComponent& comp, bool is_ui)
{
  QString path = rootPath;
  if(is_ui && path.endsWith(".qml"))
    path.insert(path.size() - 4, ".ui");
  const auto url = QUrl::fromLocalFile(path);
  QFile original{path};
  if(original.open(QIODevice::ReadOnly) && original.readAll() == str)
  {
    // Unmodified presets retain Qt's disk compilation cache and their own
    // import directory. Never merge unrelated addon files in one cache folder.
    comp.loadUrl(url);
  }
  else
  {
    // In-memory edits keep the original import base without creating files in
    // a directory that QQmlTypeLoader may already have cached.
    comp.setData(str, url);
  }
}

inline JS::Script* createJSObject(QQmlComponent& c, QQmlContext* context)
{
  const auto& errs = c.errors();
  if(!errs.empty())
  {
    ossia::logger().error(
        "Uncaught exception at line {} : {}", errs[0].line(),
        errs[0].toString().toStdString());
    return nullptr;
  }
  else
  {
    auto object = c.create(context);
    auto obj = qobject_cast<JS::Script*>(object);
    if(obj)
      return obj;
    delete object;
    return nullptr;
  }
}

inline JS::Script* createJSObject(
    const QString& rootPath, const QString& val, QQmlEngine* engine,
    QQmlContext* context)
{
  if(val.trimmed().startsWith("import"))
  {
    QQmlComponent c{engine};
    loadJSObjectFromString(rootPath, val.toUtf8(), c, false);
    return createJSObject(c, context);
  }
  else if(QFile::exists(val))
  {
    QQmlComponent c{engine, QUrl::fromLocalFile(val)};
    return createJSObject(c, context);
  }
  return nullptr;
}

inline void setupExecFuncs(auto* self, QObject* context, ossia::qt::qml_engine_functions* m_execFuncs)
{
  QObject::connect(
      m_execFuncs, &ossia::qt::qml_engine_functions::system, qApp,
      [](const QString& code) {
    std::thread{[code] { ::system(code.toStdString().c_str()); }}.detach();
  }, Qt::QueuedConnection);

  if(auto* js_panel = score::GUIAppContext().findPanel<JS::PanelDelegate>())
  {
    QObject::connect(
        m_execFuncs, &ossia::qt::qml_engine_functions::exec, js_panel,
        &JS::PanelDelegate::evaluate, Qt::QueuedConnection);

    QObject::connect(
        m_execFuncs, &ossia::qt::qml_engine_functions::compute, m_execFuncs,
        [self, context, m_execFuncs, js_panel](const QString& code, const QString& cbname) {
      // Exec thread

      // Callback ran in UI thread
      auto cb = [self
                 , context=QPointer{context}
                 , cur = QPointer{self->m_object}
                 , m_execFuncs
                 , cbname] (const QVariant& v) {
        if(!self)
          return;

        // Go back to exec thread, we have to go through the normal engine exec ctx
        ossia::qt::run_async(m_execFuncs, [self, context, cur, v, cbname] {
          if(!context || !cur)
            return;
          if(self->m_object != cur)
            return;

          auto mo = self->m_object->metaObject();
          for(int i = 0; i < mo->methodCount(); i++)
          {
            if(mo->method(i).name() == cbname)
            {
              mo->method(i).invoke(
                  self->m_object, Qt::DirectConnection, QGenericReturnArgument(),
                  QArgument<QVariant>{"v", v});
            }
          }
        });
      };

      // Go to ui thread
      ossia::qt::run_async(js_panel, [js_panel, code, cb]() {
        js_panel->compute(code, cb); // This invokes cb
      });
    }, Qt::DirectConnection);
  }
}
}
