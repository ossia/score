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
#include <QQmlAbstractUrlInterceptor>
#include <QReadWriteLock>
#include <QStandardPaths>
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

//! Path of a program's ui half: foo.qml -> foo.ui.qml.
inline QString uiPathOf(const QString& rootPath)
{
  QString path = rootPath;
  if(path.endsWith(".qml"))
    path.insert(path.size() - 4, ".ui");
  return path;
}

//! Whether `str` is what `path` holds.
//!
//! Compared trimmed: what arrives here is the process' script, trimmed on its
//! way into the model, while the file it came from almost always ends in a
//! newline. Comparing the bytes as they are meant that a script straight out of
//! a file did not count as being that file.
inline bool scriptIsItsFile(const QString& path, const QByteArray& str)
{
  QFile f{path};
  return f.open(QIODevice::ReadOnly) && f.readAll().trimmed() == str.trimmed();
}

namespace detail
{
//! What each staged folder stands in for. Read from whichever thread qml
//! resolves a url on, which is not the one that stages.
struct StagedScripts
{
  struct Entry
  {
    QString fileName;    //!< the staged script, the one file that is really there
    QString originalDir; //!< where everything else it asks for lives
  };
  QReadWriteLock lock;
  QHash<QString, Entry> byDir;
};

inline StagedScripts& stagedScripts()
{
  static StagedScripts s;
  return s;
}
}

//! Where edited scripts are put: under the cache, never in the folder they were
//! edited from. A folder of its own for each of them, because qml keeps the
//! listing it took of a folder the first time it read from it and reports a
//! file that turns up afterwards as a name-case mismatch.
inline const QString& editStagingRoot()
{
  static const QString root = [] {
    const auto cache = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    return cache.isEmpty() ? QString{} : cache + QStringLiteral("/qml-edits");
  }();
  return root;
}

//! Sends what a staged script asks of its folder back to the folder it was
//! edited from, so that its imports resolve as they did there.
//!
//! Reaches everything named outright: a .js import, a type a qmldir declares.
//! It cannot reach a bare type name -- qml looks for one of those by listing
//! the component's own folder, and gives up before there is a url to redirect.
class EditedScriptRedirect final : public QQmlAbstractUrlInterceptor
{
public:
  QUrl intercept(const QUrl& url, DataType) override
  {
    const QString& root = editStagingRoot();
    if(root.isEmpty() || !url.isLocalFile())
      return url;

    // Staged folders are the children of one root, so what is asked for is
    // <root>/<staged folder>/<what the script wants>, however deep that goes.
    const QString path = url.toLocalFile();
    if(!path.startsWith(root) || path.size() <= root.size() + 1
       || path[root.size()] != QLatin1Char('/'))
      return url;

    const QString rest = path.mid(root.size() + 1);
    const int slash = rest.indexOf(QLatin1Char('/'));
    if(slash <= 0)
      return url;

    const QString wanted = rest.mid(slash + 1);

    auto& s = detail::stagedScripts();
    QReadLocker _{&s.lock};
    const auto it = s.byDir.constFind(root + QLatin1Char('/') + rest.left(slash));
    if(it == s.byDir.cend() || wanted == it->fileName)
      return url;
    return QUrl::fromLocalFile(it->originalDir + QLatin1Char('/') + wanted);
  }
};

namespace detail
{
//! Owns one redirect for the engine it is parented to.
struct RedirectHolder : QObject
{
  EditedScriptRedirect interceptor;
  QQmlEngine* engine{};
  ~RedirectHolder() override
  {
    if(engine)
      engine->removeUrlInterceptor(&interceptor);
  }
};
}

//! Put the redirect on an engine, once. Every url the engine resolves goes
//! through it, so it does nothing but one hash lookup for anything that is not
//! under a staged folder.
inline void installEditRedirect(QQmlEngine& engine)
{
  static constexpr auto marker = "score_qml_edit_redirect";
  if(engine.property(marker).toBool())
    return;

  auto* holder = new detail::RedirectHolder;
  holder->engine = &engine;
  holder->setParent(&engine);
  engine.addUrlInterceptor(&holder->interceptor);
  engine.setProperty(marker, true);
}

//! Folders staged for one script share a prefix, so that a new edit can drop
//! what the previous one left. The folder it came from is part of it: two
//! scripts of the same name in different folders are not the same script.
inline QString stagingFolderPrefix(const QString& originalPath)
{
  const QFileInfo fi{originalPath};
  QString stem = fi.fileName();
  for(QChar& c : stem)
    if(!c.isLetterOrNumber())
      c = QLatin1Char('_');
  return stem + QLatin1Char('-')
         + QString::fromLatin1(QCryptographicHash::hash(
                                   fi.absolutePath().toUtf8(), QCryptographicHash::Sha1)
                                   .toHex()
                                   .left(8))
         + QLatin1Char('-');
}

//! Drop what earlier edits of the same script left behind. Removal may fail --
//! another process can still have a file open -- and the next edit tries again.
inline void sweepStagedFolders(
    const QString& root, const QString& prefix, const QString& keep)
{
  QDir dir{root};
  const auto stale = dir.entryList({prefix + "*"}, QDir::Dirs | QDir::NoDotAndDotDot);
  auto& s = detail::stagedScripts();
  for(const auto& name : stale)
  {
    if(name == keep)
      continue;
    const QString path = dir.filePath(name);
    if(QDir{path}.removeRecursively())
    {
      QWriteLocker _{&s.lock};
      s.byDir.remove(path);
    }
  }
}

//! Give an edited script a file of its own so that it can be compiled by url
//! like any other, and note where the rest of what it needs lives. Empty when
//! there is nowhere to write it, which is not an error: the caller compiles it
//! from the bytes instead.
inline QString stageEditedScript(const QString& originalPath, const QByteArray& str)
{
  const QString root = editStagingRoot();
  if(root.isEmpty())
    return {};

  const QFileInfo fi{originalPath};
  if(fi.fileName().isEmpty())
    return {};

  // Not the name it had: a folder lends its .qml files to whatever sits in it
  // as types, and a script staged under its own name would shadow the very
  // type it is declaring itself to be. A leading dot is not an identifier, so
  // nothing is lent.
  const QString name = QStringLiteral(".score-edit")
                       + (originalPath.endsWith(QStringLiteral(".ui.qml"))
                              ? QStringLiteral(".ui.qml")
                              : QStringLiteral(".qml"));

  const QString prefix = stagingFolderPrefix(originalPath);
  const QString folder
      = prefix
        + QString::fromLatin1(
            QCryptographicHash::hash(str, QCryptographicHash::Sha1).toHex().left(16));
  const QString dir = root + QLatin1Char('/') + folder;
  const QString file = dir + QLatin1Char('/') + name;

  if(!QFileInfo::exists(file))
  {
    if(!QDir{}.mkpath(dir))
      return {};
    QFile f{file};
    if(!f.open(QIODevice::WriteOnly))
      return {};
    const bool written = f.write(str) == str.size();
    f.close();
    if(!written)
    {
      QDir{dir}.removeRecursively();
      return {};
    }
  }

  {
    auto& s = detail::stagedScripts();
    QWriteLocker _{&s.lock};
    s.byDir.insert(dir, {name, fi.absolutePath()});
  }

  sweepStagedFolders(root, prefix, folder);
  return file;
}

inline void loadJSObjectFromString(
    const QString& rootPath, const QByteArray& str, QQmlComponent& comp, bool is_ui)
{
  const QString path = is_ui ? uiPathOf(rootPath) : rootPath;

  if(scriptIsItsFile(path, str))
  {
    loadJSObjectFromUrl(QUrl::fromLocalFile(path), str, comp);
    return;
  }

  // An edit is in no file, and only something with a file can be compiled by
  // url -- which is what lets qml hand back an already compiled form, and what
  // will let it compile off this thread. So it is given one.
  if(auto* engine = comp.engine())
  {
    if(const QString staged = stageEditedScript(path, str); !staged.isEmpty())
    {
      installEditRedirect(*engine);
      loadJSObjectFromUrl(QUrl::fromLocalFile(staged), str, comp);
      return;
    }
  }

  comp.setData(str, QUrl::fromLocalFile(path));
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
