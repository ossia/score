#include <score/tools/Environment.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>

#include <QMetaObject>
#include <QPointer>

#include <memory>

namespace score
{
namespace
{
//! Both sides of a callback pair: report the failure if anyone is listening,
//! and say nothing otherwise rather than pretending it worked.
void fail(const Environment::Callback<Environment::Failure>& onFailed, QString why)
{
  if(onFailed)
    onFailed(std::move(why));
}
}

Environment::~Environment() = default;

qint64 maxInlineTransferBytes() noexcept
{
  return 8 * 1024 * 1024;
}

namespace
{
//! One walk in flight. Shared: a listing can arrive long after the call that
//! asked for it returned, and several directories are in flight at once.
struct RecursiveWalk
{
  Environment& env;
  QString suffix;
  Environment::Callback<std::vector<DirEntry>> onListed;
  std::vector<DirEntry> found;
  int pending{};
  QPointer<QObject> context;
};

void walkDone(const std::shared_ptr<RecursiveWalk>& st)
{
  // The directory that launched its children is only finished after they are
  // all launched, so this cannot reach zero early on a local environment, where
  // every listing answers before `list` returns.
  if(--st->pending == 0 && st->onListed)
    st->onListed(std::move(st->found));
}

void listDir(const std::shared_ptr<RecursiveWalk>& st, const Uri& dir, int depth);

//! Counted before it is scheduled, so a walk that yields cannot look finished
//! in between.
void walkDir(const std::shared_ptr<RecursiveWalk>& st, const Uri& dir, int depth)
{
  st->pending++;

  if(st->context)
  {
    QMetaObject::invokeMethod(
        st->context, [st, dir, depth] { listDir(st, dir, depth); },
        Qt::QueuedConnection);
    return;
  }

  listDir(st, dir, depth);
}

void listDir(const std::shared_ptr<RecursiveWalk>& st, const Uri& dir, int depth)
{
  st->env.list(
      dir,
      [st, depth](std::vector<DirEntry> entries) {
    for(auto& e : entries)
    {
      if(e.directory)
      {
        if(depth > 0)
          walkDir(st, e.uri, depth - 1);
      }
      else if(st->suffix.isEmpty() || e.name.endsWith(st->suffix, Qt::CaseInsensitive))
      {
        st->found.push_back(std::move(e));
      }
    }
    walkDone(st);
      },
      [st](const Environment::Failure&) { walkDone(st); });
}
}

void listRecursive(
    Environment& env, const Uri& root, const QString& suffix,
    Environment::Callback<std::vector<DirEntry>> onListed, int maxDepth,
    QObject* context)
{
  auto st = std::make_shared<RecursiveWalk>(
      env, suffix, std::move(onListed), std::vector<DirEntry>{}, 0, context);
  walkDir(st, root, maxDepth);
}

LocalEnvironment::LocalEnvironment(const DocumentContext& ctx)
    : m_ctx{ctx}
{
}

LocalEnvironment::~LocalEnvironment() = default;

QString LocalEnvironment::resolve(const Uri& uri) const
{
  return uri.resolve(m_ctx);
}

void LocalEnvironment::list(
    const Uri& uri, Callback<std::vector<DirEntry>> onListed, Callback<Failure> onFailed)
{
  const auto path = resolve(uri);
  QDir dir{path};
  if(path.isEmpty() || !dir.exists())
  {
    fail(onFailed, QObject::tr("%1 is not a directory here").arg(uri.toString()));
    return;
  }

  std::vector<DirEntry> entries;
  for(const auto& info :
      dir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries, QDir::Name))
  {
    const auto name = info.fileName();
    entries.push_back(DirEntry{
        Uri{uri.scheme, uri.path.isEmpty() ? name : uri.path + '/' + name}, name,
        info.isDir(), info.isDir() ? 0 : info.size()});
  }

  if(onListed)
    onListed(std::move(entries));
}

void LocalEnvironment::read(
    const Uri& uri, Callback<QByteArray> onRead, Callback<Failure> onFailed)
{
  const auto path = resolve(uri);
  QFile f{path};
  if(path.isEmpty() || !f.exists())
  {
    fail(onFailed, QObject::tr("%1 is not here").arg(uri.toString()));
    return;
  }
  if(!f.open(QIODevice::ReadOnly))
  {
    fail(onFailed, QObject::tr("%1 cannot be read").arg(uri.toString()));
    return;
  }

  if(onRead)
    onRead(f.readAll());
}

void LocalEnvironment::write(
    const Uri& uri, QByteArray data, Done onWritten, Callback<Failure> onFailed)
{
  const auto path = resolve(uri);
  if(path.isEmpty())
  {
    fail(onFailed, QObject::tr("%1 does not point anywhere here").arg(uri.toString()));
    return;
  }

  QDir{}.mkpath(QFileInfo{path}.absolutePath());
  QFile f{path};
  if(!f.open(QIODevice::WriteOnly))
  {
    fail(onFailed, QObject::tr("%1 cannot be written").arg(uri.toString()));
    return;
  }
  if(f.write(data) != data.size())
  {
    fail(onFailed, QObject::tr("%1 could not be written in full").arg(uri.toString()));
    return;
  }

  if(onWritten)
    onWritten();
}
}
