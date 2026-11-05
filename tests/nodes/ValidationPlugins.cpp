#include "ValidationPlugins.hpp"

#include <Library/ProcessesItemModel.hpp>
#include <Media/Effect/Settings/Model.hpp>

#include <score/application/GUIApplicationContext.hpp>
#include <score/plugins/UuidKey.hpp>

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QUrl>

namespace sweep
{
ValidationPlugins::ValidationPlugins(
    const score::GUIApplicationContext& ctx,
    const std::function<bool(const std::string&)>& wanted, int timeout_ms)
{
  const QString env = qEnvironmentVariable("SCORE_VALIDATION_PLUGINS");
  const QString root = env.isEmpty() ? QStringLiteral("/usr/lib") : env;
  const QString links = QDir::temp().filePath(QStringLiteral("validation-plugins"));

  bool clap = false, vst = false, vst3 = false;
  for(const auto& p : validation_plugins)
  {
    if(!wanted(p.host))
      continue;
    auto& f = m_found.emplace_back();
    f.plugin = &p;
    const QString src = root + "/" + p.format + "/" + p.file;
    if(!QFileInfo::exists(src))
    {
      f.skip = "validation plug-in not found: " + src.toStdString() + " (from "
               + p.package + "; SCORE_VALIDATION_PLUGINS sets where it is looked for)";
      continue;
    }
    const std::string_view host{p.host};
    if(host == lv2_host)
    {
      f.data = QUrl::fromLocalFile(QFileInfo{src}.canonicalFilePath()).toString() + "/";
      continue;
    }

    const QString dir = links + "/" + p.format;
    QDir{}.mkpath(dir);
    const QString link = dir + "/" + p.file;
    const QString target = QFileInfo{src}.absoluteFilePath();
    if(QFileInfo{link}.symLinkTarget() != target)
    {
      QFile::remove(link);
      QFile::link(target, link);
    }
    clap |= host == clap_host;
    vst |= host == vst_host;
    vst3 |= host == vst3_host;
  }

  // Emptied first, so that a plug-in a previous run failed to scan is scanned
  // again: a path already known is not
  auto& set = ctx.settings<::Media::Settings::Model>();
  if(clap)
  {
    set.setClapPaths({});
    set.setClapPaths({links + "/clap"});
  }
  if(vst)
  {
    set.setVstPaths({});
    set.setVstPaths({links + "/vst"});
  }
  if(vst3)
  {
    set.setVst3Paths({});
    set.setVst3Paths({links + "/vst3"});
  }

  Library::ProcessesItemModel library{ctx, nullptr};
  auto lookup = [&] {
    bool all = true;
    for(auto& f : m_found)
    {
      if(!f.skip.empty() || !f.data.isEmpty())
        continue;
      library.iterate(QModelIndex{}, [&](const QModelIndex& idx) {
        if(!f.data.isEmpty())
          return;
        const auto& d = library.nodeFromModelIndex(idx);
        if(score::uuids::toByteArray(d.key.impl()) == f.plugin->host
           && !d.customData.isEmpty()
           && d.prettyName.contains(QLatin1String(f.plugin->name), Qt::CaseInsensitive))
          f.data = d.customData;
      });
      all &= !f.data.isEmpty();
    }
    return all;
  };
  QElapsedTimer t;
  t.start();
  while(!lookup() && t.elapsed() < timeout_ms)
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

  for(auto& f : m_found)
    if(f.skip.empty() && f.data.isEmpty())
      f.skip = std::string("the scan of ") + f.plugin->file
               + " listed nothing in the library";
}
}
