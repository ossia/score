#include <Process/Drop/ProcessDropHandler.hpp>

#include <score/tools/File.hpp>

#include <ossia/detail/algorithms.hpp>

#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QUrl>
namespace Process
{

ProcessDropHandler::ProcessDropHandler() { }

ProcessDropHandler::~ProcessDropHandler() { }

void ProcessDropHandler::getCustomDrops(
    std::vector<ProcessDropHandler::ProcessDrop>& drops, const QMimeData& mime,
    const score::DocumentContext& ctx) const noexcept
{
  // dropCustom is no longer noexcept (some overrides invoke parsers that
  // can throw on malformed input — see ProcessDropHandler.hpp). Catch
  // here so a throwing handler never escapes through the noexcept
  // public API and tears down the editor.
  try
  {
    dropCustom(drops, mime, ctx);
  }
  catch(const std::exception& e)
  {
    qWarning() << "ProcessDropHandler::dropCustom threw:" << e.what();
  }
  catch(...)
  {
    qWarning() << "ProcessDropHandler::dropCustom threw an unknown exception";
  }
}

void ProcessDropHandler::getMimeDrops(
    std::vector<ProcessDropHandler::ProcessDrop>& drops, const QMimeData& mime,
    const QString& fmt, const score::DocumentContext& ctx) const noexcept
{
  qDebug() << fmt << mime.data(fmt);
  auto df = DroppedFile{{}, mime.data(fmt)};
  dropData(drops, df, ctx);
}

void ProcessDropHandler::getFileDrops(
    std::vector<ProcessDropHandler::ProcessDrop>& drops, const QMimeData& mime,
    const score::FilePath& path, const score::DocumentContext& ctx) const noexcept
{
  // Check for handling through paths
  auto old_sz = drops.size();
  dropPath(drops, path, ctx);
  if(drops.size() != old_sz)
    return;

  // Fall back to manual handling
  if(QFile file{path.absolute}; file.open(QIODevice::ReadOnly))
  {
    dropData(drops, {path, file.readAll()}, ctx);
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
    std::vector<ProcessDropHandler::ProcessDrop>&, const QMimeData& data,
    const score::DocumentContext& ctx) const
{
}

void ProcessDropHandler::dropPath(
    std::vector<ProcessDropHandler::ProcessDrop>&, const score::FilePath& data,
    const score::DocumentContext& ctx) const noexcept
{
}

void ProcessDropHandler::dropData(
    std::vector<ProcessDropHandler::ProcessDrop>&, const DroppedFile& data,
    const score::DocumentContext& ctx) const noexcept
{
}

ProcessDropHandlerList::~ProcessDropHandlerList() { }

std::vector<ProcessDropHandler::ProcessDrop> ProcessDropHandlerList::getDrop(
    const QMimeData& originalMime, const score::DocumentContext& ctx) const noexcept
{
  std::vector<ProcessDropHandler::ProcessDrop> res;

  initCaches();

#if defined(__EMSCRIPTEN__)
  // On wasm, Qt writes dropped files to a transient /qt/tmp/ dir and deletes
  // them immediately after this callback returns. Move each dropped file into
  // score's persistent import area up front and rewrite the URLs, so BOTH the
  // custom-drop path (which re-reads mime.urls() itself, e.g. Sound) and the
  // file-extension path see a path that survives the drop and can be re-opened
  // later (undo/redo, reload of the process).
  QMimeData stagedMime;
  bool didStage = false;
  {
    QList<QUrl> newUrls;
    for(const QUrl& url : originalMime.urls())
    {
      // On the JSPI build (which we use), Qt hands dropped files a
      // "weblocalfile:/N/name" URL backed by the JS File object; QUrl::toLocalFile()
      // is empty but QFile can read it through QWasmFileEngine. On non-asyncify
      // builds files land at /qt/tmp instead. In both cases read the bytes here
      // (while the source is alive) and copy them into a real, persistent MEMFS
      // path so the fopen-based decoders (ffmpeg / sndfile) can open them later.
      QString src;
      if(const QString local = url.toLocalFile();
         !local.isEmpty() && QFileInfo::exists(local))
        src = local;
      else if(url.scheme() == QLatin1String("weblocalfile"))
        src = url.toString();

      if(!src.isEmpty())
      {
        if(QFile f{src}; f.open(QIODevice::ReadOnly))
        {
          const QByteArray bytes = f.readAll();
          f.close();
          QString name = url.fileName();
          if(name.isEmpty())
            name = QFileInfo{src}.fileName();
          if(QString staged = score::stageImportedFile(name, bytes); !staged.isEmpty())
          {
            newUrls.push_back(QUrl::fromLocalFile(staged));
            didStage = true;
            continue;
          }
        }
      }
      newUrls.push_back(url);
    }
    if(didStage)
    {
      for(const QString& fmt : originalMime.formats())
        stagedMime.setData(fmt, originalMime.data(fmt));
      stagedMime.setUrls(newUrls);
    }
  }
  const QMimeData& mime = didStage ? stagedMime : originalMime;
#else
  const QMimeData& mime = originalMime;
#endif

  auto handleCustomDrop = [&](Process::ProcessDropHandler& handler) {
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
    // qDebug() << "handled through getMimeDrops";
    return res;
  }

  // Look for drop handlers with the available file extensions
  for(const auto& url : mime.urls())
  {
    auto path = url.toLocalFile();

    QFileInfo f{path};
    if(f.exists())
    {
      auto ext = f.suffix().toLower();
      if(auto it = m_perFileExtension.find(ext.toStdString());
         it != m_perFileExtension.end())
      {
        auto& handler = *it->second;

        // First check if a custom drop is in order, which handles everything.
        if(handleCustomDrop(handler))
        {
          // qDebug() << "handled through getCustomDrops";
          return res;
        }

        // Then fall back to the normal mime data drop
        score::FilePath p{
            .absolute = path,
            .relative = score::relativizeFilePath(path, ctx),
            .filename = f.fileName(),
            .basename = f.baseName()};
        handler.getFileDrops(res, mime, p, ctx);
      }
    }
  }

  // TODO Fix Sound::DropHandler::drop so that we don't need to do that
  // and remove the custom drop mechanism above (problem is that customData is empty)
  /*
  {
    auto comp = [](auto& lhs, auto& rhs) {
      return lhs.creation.customData < rhs.creation.customData;
    };
    ossia::remove_duplicates(res, comp);
  }
  if(!res.empty())
  {
    qDebug() << "handled through getFileDrops";
    return res;
  }
  */
  return res;
}

std::optional<TimeVal> ProcessDropHandlerList::getMaxDuration(
    const std::vector<ProcessDropHandler::ProcessDrop>& res)
{
  using drop_t = Process::ProcessDropHandler::ProcessDrop;
  SCORE_ASSERT(!res.empty());

  auto max_t
      = std::max_element(res.begin(), res.end(), [](const drop_t& l, const drop_t& r) {
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
