#include <Process/Drop/ProcessDropHandler.hpp>

#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QUrl>
namespace Process
{

ProcessDropHandler::ProcessDropHandler() { }

ProcessDropHandler::~ProcessDropHandler() { }

std::vector<ProcessDropHandler::ProcessDrop> ProcessDropHandler::getDrops(
    const QMimeData& mime,
    const score::DocumentContext& ctx) const noexcept
{
  std::vector<ProcessDrop> res;
  // Try to extract data from the mime formats
  const auto& formats = mime.formats();
  auto commonFormats = mimeTypes().intersect(QSet(formats.begin(), formats.end()));
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

QSet<QString> ProcessDropHandler::mimeTypes() const noexcept
{
  return {};
}

QSet<QString> ProcessDropHandler::fileExtensions() const noexcept
{
  return {};
}

std::vector<ProcessDropHandler::ProcessDrop>
ProcessDropHandler::drop(const QMimeData& data, const score::DocumentContext& ctx) const noexcept
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
  for (Process::ProcessDropHandler& h : *this)
  {
    if (res = h.getDrops(mime, ctx); !res.empty())
      break;
  }
  return res;
}

std::optional<TimeVal>
ProcessDropHandlerList::getMaxDuration(const std::vector<ProcessDropHandler::ProcessDrop>& res)
{
  using drop_t = Process::ProcessDropHandler::ProcessDrop;
  SCORE_ASSERT(!res.empty());

  auto max_t = std::max_element(res.begin(), res.end(), [](const drop_t& l, const drop_t& r) {
    return l.duration < r.duration;
  });

  return max_t->duration;
}
}
