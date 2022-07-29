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

void ProcessDropHandler::getCustomDrops(
    std::vector<ProcessDropHandler::ProcessDrop>& drops,
    const QMimeData& mime,
    const score::DocumentContext& ctx) const noexcept
{
  // Check for special mime handling code
  return dropCustom(drops, mime, ctx);
}

void ProcessDropHandler::getMimeDrops(
    std::vector<ProcessDropHandler::ProcessDrop>& drops,
    const QMimeData& mime,
    const QString& fmt,
    const score::DocumentContext& ctx) const noexcept
{
  qDebug() << fmt << mime.data(fmt);
  auto df = DroppedFile{{}, mime.data(fmt)};
  dropData(drops, df, ctx);
}

void ProcessDropHandler::getFileDrops(
    std::vector<ProcessDropHandler::ProcessDrop>& drops,
    const QMimeData& mime,
    const QString& path,
    const score::DocumentContext& ctx) const noexcept
{
  // Check for handling through paths
  auto old_sz = drops.size();
  dropPath(drops, path, ctx);
  if(drops.size() != old_sz)
    return;

  // Fall back to manual handling
  if (QFile file{path}; file.open(QIODevice::ReadOnly))
  {
    dropData(drops, {QFileInfo{file}.absoluteFilePath(), file.readAll()}, ctx);
  }
}

QSet<QString> ProcessDropHandler::mimeTypes() const noexcept
{
  return {};
}

QSet<QString> ProcessDropHandler::fileExtensions() const noexcept
{
  return {};
}

void ProcessDropHandler::dropCustom(
    std::vector<ProcessDropHandler::ProcessDrop>&,
    const QMimeData& data,
    const score::DocumentContext& ctx) const noexcept
{
}

void ProcessDropHandler::dropPath(
    std::vector<ProcessDropHandler::ProcessDrop>&,
    const QString& data,
    const score::DocumentContext& ctx) const noexcept
{
}

void ProcessDropHandler::dropData(
    std::vector<ProcessDropHandler::ProcessDrop>&,
    const DroppedFile& data,
    const score::DocumentContext& ctx) const noexcept
{
}

ProcessDropHandlerList::~ProcessDropHandlerList() { }

std::vector<ProcessDropHandler::ProcessDrop> ProcessDropHandlerList::getDrop(
    const QMimeData& mime,
    const score::DocumentContext& ctx) const noexcept
{
  std::vector<ProcessDropHandler::ProcessDrop> res;

  initCaches();

  auto handleCustomDrop = [&] (auto& handler) {
    auto before = res.size();
    handler.getCustomDrops(res, mime, ctx);
    auto after = res.size();
    return after != before;
  };

  // Look for drop handlers with the available MIME types
  for(const auto& fmt : mime.formats())
  {
    if(auto it = m_perMimeTypes.find(fmt.toStdString()); it != m_perMimeTypes.end())
    {
      auto& handler = *it->second;

      // First check if a custom drop is in order, which handles everything.
      if(handleCustomDrop(handler))
        return res;

      // Then fall back to the normal mime data drop
      handler.getMimeDrops(res, mime, fmt, ctx);
    }
  }

  if(!res.empty())
  {
    qDebug() << "handled through getMimeDrops";
    return res;
  }

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
        auto& handler = *it->second;

        // First check if a custom drop is in order, which handles everything.
        if(handleCustomDrop(handler))
          return res;

        // Then fall back to the normal mime data drop
        handler.getFileDrops(res, mime, path, ctx);
      }
    }
  }

  // TODO Fix Sound::DropHandler::drop so that we don't need to do that
  {
    auto comp = [] (auto& lhs, auto& rhs) {
      return lhs.creation.customData < rhs.creation.customData;
    };
    ossia::remove_duplicates(res, comp);
  }
  if(!res.empty())
  {
    qDebug() << "handled through getFileDrops";
    return res;
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
