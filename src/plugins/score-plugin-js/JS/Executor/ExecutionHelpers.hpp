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

#include <algorithm>
#include <vector>

#include <QCryptographicHash>
#include <QPointer>
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


namespace detail
{
//! Whether the loader may be asked for `url` in this engine: either it has not
//! compiled that url yet, or it compiled exactly these bytes. The type loader
//! keeps one compiled type per url for as long as the engine lives and never
//! goes back to the file, so asking it for a url whose file has since changed
//! hands back the script it compiled the first time.
//!
//! Records the bytes on the first answer, which is the call that goes on to
//! compile them. A record may only be dropped once its engine is gone: an
//! engine that is still around still holds what it compiled. Engines are the
//! gui's or thread-local to an execution thread and are each used from one
//! thread only, so the records are kept per thread and need no lock.
inline bool loaderIsCurrent(QQmlEngine& e, const QUrl& url, const QByteArray& str)
{
  using Record = std::pair<QPointer<QQmlEngine>, QHash<QUrl, QByteArray>>;
  thread_local std::vector<Record> known;

  auto it = std::find_if(
      known.begin(), known.end(), [&](const Record& r) { return r.first == &e; });
  if(it == known.end())
  {
    std::erase_if(known, [](const Record& r) { return r.first.isNull(); });
    it = known.emplace(known.end(), &e, QHash<QUrl, QByteArray>{});
  }

  const auto digest = QCryptographicHash::hash(str, QCryptographicHash::Sha1);
  auto& digests = it->second;
  const auto known_digest = digests.constFind(url);
  if(known_digest == digests.cend())
  {
    digests.insert(url, digest);
    return true;
  }
  return *known_digest == digest;
}
}

//! Compile the script at `url`, whose current contents are `str`.
//!
//! Going through the loader is what lets Qt hand back an already compiled form
//! -- from this engine, or from its on-disk cache -- instead of parsing the
//! script again. The execution and render threads both come through here, and
//! one thread-local engine is shared by every node on that thread, so several
//! instances of one preset compile it once between them.
inline void loadJSObjectFromUrl(
    const QUrl& url, const QByteArray& str, QQmlComponent& comp)
{
  if(auto* engine = comp.engine(); engine && detail::loaderIsCurrent(*engine, url, str))
    comp.loadUrl(url);
  else
    comp.setData(str, url);
}

inline void loadJSObjectFromString(
    const QString& rootPath, const QByteArray& str, QQmlComponent& comp, bool is_ui)
{
  QString path = rootPath;
  if(is_ui && path.endsWith(".qml"))
    path.insert(path.size() - 4, ".ui");
  const auto url = QUrl::fromLocalFile(path);

  // Compared trimmed: what arrives here is the process' script, which is
  // trimmed on its way into the model, while the file it came from almost
  // always ends in a newline. Comparing the bytes as they are meant that a
  // script straight out of a file did not count as being that file, and the
  // loader -- the whole point of naming the file at all -- was skipped for
  // every one of them.
  QFile original{path};
  if(original.open(QIODevice::ReadOnly) && original.readAll().trimmed() == str.trimmed())
    loadJSObjectFromUrl(url, str, comp);
  else
    // An in-memory edit is not in any file: it keeps the original import base
    // without the loader ever seeing it.
    comp.setData(str, url);
}

//! Compile a script that lives in a file, through the loader where that is
//! still current. See loadJSObjectFromUrl.
inline void loadJSObjectFromFile(const QString& path, QQmlComponent& comp)
{
  QFile f{path};
  if(!f.open(QIODevice::ReadOnly))
    return;
  loadJSObjectFromUrl(QUrl::fromLocalFile(path), f.readAll(), comp);
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
    QQmlComponent c{engine};
    loadJSObjectFromFile(val, c);
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
