#include <Process/Drop/ProcessDropHandler.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QUrl>
namespace Process
{

ProcessDropHandler::ProcessDropHandler() { }

ProcessDropHandler::~ProcessDropHandler() { }

std::vector<ProcessDropHandler::ProcessDrop> ProcessDropHandler::getMimeDrops(
    const QMimeData& mime,
    const QString& fmt,
    const score::DocumentContext& ctx) const noexcept
{
  std::vector<ProcessDrop> res;

  // First check for special mime handling code
  res = drop(mime, ctx);
  if (!res.empty())
    return res;

  // Fall back to manual handling
  std::vector<DroppedFile> data;
  data.push_back({{}, mime.data(fmt)});
  res = dropData(data, ctx);

  return res;
}

std::vector<ProcessDropHandler::ProcessDrop> ProcessDropHandler::getFileDrops(
    const QMimeData& mime,
    const QString& path,
    const score::DocumentContext& ctx) const noexcept
{
  std::vector<ProcessDrop> res;

  // First check for special mime handling code
  res = drop(mime, ctx);
  if (!res.empty())
    return res;

  // Then check for handling through paths
  res = dropPaths({path}, ctx);
  if(!res.empty())
    return res;

  // Fall back to manual handling
  std::vector<DroppedFile> data;
  {
    QFile file{path};
    if (file.open(QIODevice::ReadOnly))
    {
      data.push_back({QFileInfo{file}.absoluteFilePath(), file.readAll()});
    }
  }
  res = dropData(data, ctx);

  return res;
}
/*
std::vector<ProcessDropHandler::ProcessDrop> ProcessDropHandler::getDrops(
    const QMimeData& mime,
    const score::DocumentContext& ctx) const noexcept
{
  std::vector<ProcessDrop> res;
  // Try to extract data from the mime formats
  const auto& formats = mime.formats();
  auto commonFormats = mimeTypes().intersect(
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
      formats.toSet()
#else
      QSet<QString>(formats.begin(), formats.end())
#endif
  );
  if (!commonFormats.isEmpty())
  {
    // Try to check if the handler has special code for handling mime data

    res = drop(mime, ctx);
    if (!res.empty())
      return res;

    std::vector<DroppedFile> data;
    for (auto& format : commonFormats)
    {
      data.push_back({{}, mime.data(format)});
    }
    res = dropData(data, ctx);
    if (!res.empty())
      return res;
  }

  // Else try to extract data from actual files
  const auto& urls = mime.urls();
  if (!urls.empty())
  {
    std::vector<QString> paths;
    const auto& accepted_ext = fileExtensions();
    for (const auto& url : urls)
    {
      auto path = url.toLocalFile();
      QFileInfo f{path};
      if (f.exists() && accepted_ext.contains(f.suffix().toLower()))
      {
        paths.push_back(std::move(path));
      }
    }
    if (!paths.empty())
    {
      res = drop(mime, ctx);
      if (!res.empty())
        return res;
    }

    std::vector<DroppedFile> data;
    for (const auto& path : paths)
    {
      QFile file{path};
      if (file.open(QIODevice::ReadOnly))
      {
        data.push_back({QFileInfo{file}.absoluteFilePath(), file.readAll()});
      }
    }
    res = dropData(data, ctx);
    if (!res.empty())
      return res;
  }
  return res;
}
*/

QSet<QString> ProcessDropHandler::mimeTypes() const noexcept
{
  return {};
}

QSet<QString> ProcessDropHandler::fileExtensions() const noexcept
{
  return {};
}

std::vector<ProcessDropHandler::ProcessDrop> ProcessDropHandler::drop(
    const QMimeData& data,
    const score::DocumentContext& ctx) const noexcept
{
  return {};
}

std::vector<ProcessDropHandler::ProcessDrop> ProcessDropHandler::dropPaths(
    const std::vector<QString>& data,
    const score::DocumentContext& ctx) const noexcept
{
  return {};
}

std::vector<ProcessDropHandler::ProcessDrop> ProcessDropHandler::dropData(
    const std::vector<DroppedFile>& data,
    const score::DocumentContext& ctx) const noexcept
{
  return {};
}

ProcessDropHandlerList::~ProcessDropHandlerList() { }

std::vector<ProcessDropHandler::ProcessDrop> ProcessDropHandlerList::getDrop(
    const QMimeData& mime,
    const score::DocumentContext& ctx) const noexcept
{
  std::vector<ProcessDropHandler::ProcessDrop> res;

  initCaches();

  // Look for drop handlers with the available MIME types
  for(const auto& fmt : mime.formats())
  {
    if(auto it = m_perMimeTypes.find(fmt.toStdString()); it != m_perMimeTypes.end())
    {
      ossia::insert_at_end(res, it->second->getMimeDrops(mime, fmt, ctx));
    }
  }

  if(!res.empty())
    return res;

  // Look for drop handlers with the available file extensions
  for(const auto& url : mime.urls())
  {
    auto path = url.toLocalFile();
    QFileInfo f{path};
    if (f.exists())
    {
      auto ext = f.suffix().toLower();
      if(auto it = m_perFileExtension.find(ext.toStdString()); it != m_perFileExtension.end())
      {
        ossia::insert_at_end(res, it->second->getFileDrops(mime, path, ctx));
        qDebug() << res.size();
      }
    }
  }

  // TODO Fix Sound::DropHandler::drop so that we don't need to do that
  {
    std::sort(res.begin(), res.end(), [] (auto& lhs, auto& rhs) {
      return lhs.creation.customData < rhs.creation.customData;
    });
    std::unique(res.begin(), res.end(), [] (auto& lhs, auto& rhs) {
      return lhs.creation.customData < rhs.creation.customData;
    });
  }
  //
  //   if(!res.empty())
  //     return res;
  //
  //   // Look up the old way
  //   for (Process::ProcessDropHandler& h : *this)
  //   {
  //     if (res = h.getDrops(mime, ctx); !res.empty())
  //       break;
  //   }
  return res;
}

std::optional<TimeVal> ProcessDropHandlerList::getMaxDuration(
    const std::vector<ProcessDropHandler::ProcessDrop>& res)
{
  using drop_t = Process::ProcessDropHandler::ProcessDrop;
  SCORE_ASSERT(!res.empty());

  auto max_t = std::max_element(
      res.begin(), res.end(), [](const drop_t& l, const drop_t& r) {
        return l.duration < r.duration;
      });

  return max_t->duration;
}

void ProcessDropHandlerList::initCaches() const
{
  if(m_lastCacheSize != this->size())
  {
    m_perMimeTypes.clear();
    m_perFileExtension.clear();

    for(auto& handler : *this)
    {
      for(const auto& ext : handler.fileExtensions())
      {
        m_perFileExtension[ext.toLower().toStdString()] = &handler;
      }

      for(const auto& ext : handler.mimeTypes())
      {
        m_perMimeTypes[ext.toLower().toStdString()] = &handler;
      }
    }
  }
}
}
