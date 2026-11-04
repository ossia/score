#include "DefaultLibrary.hpp"

#include <core/document/DocumentTemplates.hpp>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::DefaultLibrary)

namespace score
{
DefaultLibrary::DefaultLibrary() = default;

DefaultLibrary& DefaultLibrary::instance()
{
  static DefaultLibrary lib;
  return lib;
}

QUuid DefaultLibrary::key()
{
  return QUuid::fromString(QLatin1String{"c0ef5539-774c-477c-9989-234da83f284f"});
}

QString DefaultLibrary::folder()
{
  const auto root = libraryRootPath();
  if(root.isEmpty())
    return {};
  return root + QStringLiteral("/packages/default");
}

int DefaultLibrary::installedVersion()
{
  const auto dir = folder();
  if(dir.isEmpty())
    return -1;

  QFile f{dir + QStringLiteral("/package.json")};
  if(!f.open(QIODevice::ReadOnly))
    return -1;

  const auto obj = QJsonDocument::fromJson(f.readAll()).object();
  return obj.value(QLatin1String{"version"}).toInt(0);
}

DefaultLibrary::State DefaultLibrary::state() const
{
  if(m_installing)
    return State::Installing;
  if(m_failed)
    return State::Failed;

  const int installed = installedVersion();
  if(installed < 0)
    return State::Missing;
  if(m_available > installed)
    return State::UpdateAvailable;
  return State::Current;
}

void DefaultLibrary::setAvailable(int version, qint64 bytes)
{
  m_available = version;
  m_size = bytes;
  changed();
}

void DefaultLibrary::setInstaller(std::function<void()> installer)
{
  m_installer = std::move(installer);
  changed();
}

void DefaultLibrary::install()
{
  if(m_installing || !m_installer)
    return;

  m_installing = true;
  m_fromView = offeredInView();
  m_failed = false;
  m_error.clear();
  m_progress = -1.;
  changed();

  // The installer may finish synchronously and report through setFinished().
  auto installer = m_installer;
  installer();
}

void DefaultLibrary::setProgress(qint64 received, qint64 total)
{
  m_installing = true;
  m_failed = false;
  m_progress = total > 0 ? std::clamp(double(received) / double(total), 0., 1.) : -1.;
  changed();
}

void DefaultLibrary::setFinished(bool ok, const QString& error)
{
  m_installing = false;
  m_fromView = false;
  m_failed = !ok;
  m_error = ok ? QString{} : error;
  m_progress = -1.;
  changed();
}
}
