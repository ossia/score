#include <score/tools/Environment.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QObject>

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
