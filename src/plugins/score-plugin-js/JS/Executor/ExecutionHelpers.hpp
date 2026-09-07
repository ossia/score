#pragma once
#include <JS/Commands/EditScript.hpp>
#include <JS/ConsolePanel.hpp>
#include <JS/Qml/QmlObjects.hpp>
#include <Library/LibrarySettings.hpp>

#include <score/application/ApplicationContext.hpp>
#include <score/application/GUIApplicationContext.hpp>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <score/document/DocumentInterface.hpp>

#include <ossia/detail/logger.hpp>

#include <ossia-qt/invoke.hpp>
#include <ossia-qt/qml_engine_functions.hpp>

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

inline void connectStateCommit(Script* script, ProcessModel* model)
{
  if(!model)
    return;
  QObject::connect(
      script, &Script::commitState, model,
      [context = QPointer{model}](const QString& key, const QJSValue& value) {
    auto converted = ossia::qt::value_from_js(value);
    QMetaObject::invokeMethod(qApp, [context, key, value = std::move(converted)] {
      if(!context)
        return;
      const auto& state = context->state();
      if(auto it = state.find(key); it != state.end() && it->second == value)
        return;
      CommandDispatcher<> dispatcher{
          score::IDocument::documentContext(*context).commandStack};
      dispatcher.submit<UpdateStateElement>(*context, key, value);
    }, Qt::QueuedConnection);
  }, Qt::DirectConnection);
}

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
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QString>

inline bool copyDirectoryRecursively(const QString& sourcePath, const QString& destPath)
{
  QDir sourceDir(sourcePath);
  if(!sourceDir.exists())
  {
    return false;
  }

  QDir destDir(destPath);
  // Create the destination directory if it doesn't exist
  if(!destDir.exists() && !destDir.mkpath("."))
  {
    return false;
  }

  bool success = true;

  // Get all files and directories, including hidden and system files, excluding "." and ".."
  const QFileInfoList entries = sourceDir.entryInfoList(
      QDir::NoDotAndDotDot | QDir::AllEntries | QDir::Hidden | QDir::System);

  for(const QFileInfo& entryInfo : entries)
  {
    QString newDestPath = destDir.absoluteFilePath(entryInfo.fileName());

    if(entryInfo.isDir())
    {
      // Recursively copy subdirectories
      if(!copyDirectoryRecursively(entryInfo.absoluteFilePath(), newDestPath))
      {
        success = false;
      }
    }
    else
    {
      // Overwrite existing files at the destination
      if(QFile::exists(newDestPath))
      {
        QFile::remove(newDestPath);
      }
      // Copy the file
      if(!QFile::copy(entryInfo.absoluteFilePath(), newDestPath))
      {
        success = false;
      }
    }
  }

  return success;
}

inline bool copyParentFolderContents(const QString& rootPath, const QString& dst)
{
  QFileInfo fileInfo(rootPath);

  QString parentFolder = fileInfo.absolutePath();

  return copyDirectoryRecursively(parentFolder, dst);
}

// Write str to a cache file on disk so that Qt's QML compilation cache can be used.
// Returns the cache file path on success, empty string on failure.
inline QString ensureJSCacheFile(const QByteArray& str, bool is_ui)
{
#if __has_include(<boost/hash2/xxh3.hpp>)
  static const auto cache_path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  QString path = cache_path + "/Script" + hashFileData(str) + (is_ui ? ".ui.qml" : ".qml");
  QFile f{path};
  if(f.open(QIODevice::ReadWrite))
  {
    if(str != f.readAll())
    {
      f.resize(0);
      f.reset();
      f.write(str);
      f.flush();
    }
    f.close();
    return path;
  }
#endif
  return {};
}

inline void loadJSObjectFromString(
    const QString& rootPath, const QByteArray& str, QQmlComponent& comp, bool is_ui)
{
  auto path = ensureJSCacheFile(str, is_ui);
  if(!path.isEmpty())
  {
    static const auto cache_path = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    copyParentFolderContents(rootPath, cache_path);

    comp.loadUrl(QUrl::fromLocalFile(path));
  }
  else
  {
    comp.setData(str, QUrl::fromLocalFile(rootPath));
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
