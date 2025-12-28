#pragma once
#include <JS/Qml/QmlObjects.hpp>
#include <Library/LibrarySettings.hpp>
#include <score/application/ApplicationContext.hpp>
#include <score/application/GUIApplicationContext.hpp>

#include <ossia/detail/logger.hpp>
#include <ossia-qt/invoke.hpp>
#include <ossia-qt/qml_engine_functions.hpp>
#include <JS/ConsolePanel.hpp>

#include <QDir>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QStandardPaths>
#include <QUrl>

#if __has_include(<boost/hash2/xxh3.hpp>)
#include <boost/hash2/xxh3.hpp>
#include <boost/algorithm/hex.hpp>
#endif

namespace JS
{

static inline QString hashFileData(const QByteArray& str)
{
  QString hexName;
#if __has_include(<boost/hash2/xxh3.hpp>)
  boost::hash2::xxh3_128 hasher;
  hasher.update(str.constData(), str.size());
  const auto result = hasher.result();
  std::string hexString;
  boost::algorithm::hex(result.begin(), result.end(), std::back_inserter(hexString));

  hexName.reserve(32);
  hexName.push_back("-");
  hexName.append(hexString.data());
#endif
  return hexName;
}

inline void loadJSObjectFromString(const QByteArray& str, QQmlComponent& comp, bool is_ui)
{
  static const auto& lib = score::AppContext().settings<Library::Settings::Model>();
#if __has_include(<boost/hash2/xxh3.hpp>)
  static const auto cache_path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  QFile f{cache_path + "/Script" + hashFileData(str) + (is_ui ? ".ui.qml" : ".qml")};
  if(f.open(QIODevice::ReadWrite))
  {
    // Only way to make sure we hit the QML cache
    if(str != f.readAll())
    {
      f.resize(0);
      f.reset();
      f.write(str);
      f.flush();
      f.close();
    }
    comp.loadUrl(QUrl::fromLocalFile(f.fileName()));
  }
  else
#endif
  {
    QString path = lib.getDefaultLibraryPath() + QDir::separator() + "Scripts"
                   + QDir::separator() + "include" + QDir::separator() + "Script" + hashFileData(str) + ".qml";
    comp.setData(str, QUrl::fromLocalFile(path));
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

inline JS::Script* createJSObject(const QString& val, QQmlEngine* engine, QQmlContext* context)
{
  if(val.trimmed().startsWith("import"))
  {
    QQmlComponent c{engine};
    loadJSObjectFromString(val.toUtf8(), c, false);
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
