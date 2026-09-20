#include "OnlineExamples.hpp"

#include <score/tools/Platforms.hpp>

#include <core/document/DocumentTemplates.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QSaveFile>
#include <QSettings>
#include <QStringList>
#include <QTimer>
#include <QStandardPaths>
#include <QUrl>

#include <zipdownloader.hpp>

#include <algorithm>

#include <wobjectimpl.h>
W_OBJECT_IMPL(score::OnlineExamples)

namespace score
{
namespace
{
// Must outlive its replies, and keeps the connection pool.
QNetworkAccessManager& network()
{
  static QNetworkAccessManager mgr;
  return mgr;
}

QNetworkRequest makeRequest(const QUrl& url)
{
  QNetworkRequest req{url};
  req.setAttribute(QNetworkRequest::HttpPipeliningAllowedAttribute, true);
  req.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
  req.setAttribute(
      QNetworkRequest::RedirectPolicyAttribute,
      QNetworkRequest::NoLessSafeRedirectPolicy);
#if !defined(__EMSCRIPTEN__)
  req.setRawHeader("User-Agent", "ossia-score");
#endif
  return req;
}

QString jsonString(const QJsonObject& obj, const char* key)
{
  const auto v = obj.value(QLatin1String{key});
  return v.isString() ? v.toString() : QString{};
}
}

OnlineExamples::OnlineExamples(QObject* parent)
    : QObject{parent}
{
}

OnlineExamples::~OnlineExamples() = default;

QString OnlineExamples::manifestUrl()
{
  if(auto env = qEnvironmentVariable("SCORE_EXAMPLES_MANIFEST"); !env.isEmpty())
    return env;

  return QStringLiteral("https://ossia.io/score-docs/assets/scores/index.json");
}

QString OnlineExamples::installRoot()
{
  const auto root = libraryRootPath();
  if(root.isEmpty())
    return {};
  return root + QStringLiteral("/Examples/score-docs");
}

QString OnlineExamples::cacheFolder()
{
  const auto base = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
  return base.isEmpty() ? QString{} : base + QStringLiteral("/score-docs");
}

QString OnlineExamples::installFolder(const OnlineExample& ex)
{
  const auto root = installRoot();
  if(root.isEmpty() || ex.id.isEmpty())
    return {};
  return root + "/" + ex.id;
}

QString OnlineExamples::localPath(const OnlineExample& ex)
{
  const auto folder = installFolder(ex);
  if(folder.isEmpty())
    return {};

  if(ex.format != QLatin1String{"zip"})
    return folder + "." + ex.format;

  const auto scores
      = QDir{folder}.entryList({"*.score", "*.scorejson"}, QDir::Files, QDir::Name);
  return scores.isEmpty() ? QString{} : QDir{folder}.filePath(scores.front());
}

void OnlineExamples::parse(const QByteArray& json)
{
  QJsonParseError err{};
  const auto doc = QJsonDocument::fromJson(json, &err);
  if(err.error != QJsonParseError::NoError || !doc.isObject())
    return;

  const auto obj = doc.object();
  if(obj.value("version").toInt() != 1)
    return;

  std::vector<OnlineExample> out;
  const auto arr = obj.value("examples").toArray();
  out.reserve(arr.size());
  for(const auto& v : arr)
  {
    if(!v.isObject())
      continue;
    const auto e = v.toObject();

    OnlineExample ex;
    ex.id = jsonString(e, "id");
    ex.file = jsonString(e, "file");
    ex.format = jsonString(e, "format");
    if(ex.id.isEmpty() || ex.file.isEmpty() || ex.format.isEmpty())
      continue;
    // The id becomes a path under the library.
    if(ex.id.contains("..") || ex.id.startsWith('/'))
      continue;

    ex.name = jsonString(e, "name");
    ex.description = jsonString(e, "description");
    ex.section = jsonString(e, "section");
    ex.group = jsonString(e, "group");
    ex.category = jsonString(e, "category");
    ex.page = jsonString(e, "page");
    ex.image = jsonString(e, "image");

    QStringList platforms;
    for(const auto& p : e.value("platforms").toArray())
      if(p.isString())
        platforms.push_back(p.toString());
    if(!runsOnThisPlatform(platforms.join(' ')))
      continue;

    ex.size = qint64(e.value("size").toDouble());
    out.push_back(std::move(ex));
  }

  m_examples = std::move(out);
}

void OnlineExamples::loadCache()
{
#if defined(__EMSCRIPTEN__)
  QSettings s;
  if(const auto json = s.value("score-docs/manifest").toByteArray(); !json.isEmpty())
    parse(json);
  m_etag = s.value("score-docs/etag").toString();
#else
  const auto folder = cacheFolder();
  if(folder.isEmpty())
    return;

  QFile f{folder + "/index.json"};
  if(f.open(QIODevice::ReadOnly))
    parse(f.readAll());

  QFile e{folder + "/index.etag"};
  if(e.open(QIODevice::ReadOnly))
    m_etag = QString::fromUtf8(e.readAll()).trimmed();
#endif
}

void OnlineExamples::refresh()
{
  refresh(!m_etag.isEmpty());
}

void OnlineExamples::refresh(bool conditional)
{
  auto req = makeRequest(QUrl{manifestUrl()});
  if(conditional)
    req.setRawHeader("If-None-Match", m_etag.toUtf8());

  auto reply = network().get(req);
  connect(reply, &QNetworkReply::finished, this, [this, reply, conditional] {
    reply->deleteLater();
    if(reply->error() != QNetworkReply::NoError)
    {
      // If-None-Match is not CORS-safelisted, so a site that answers the
      // preflight without allowing it refuses the conditional request but
      // would serve the plain one.
      if(conditional)
        refresh(false);
      return;
    }

    const int status
        = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if(status == 304)
      return;

    const auto body = reply->readAll();
    if(body.isEmpty())
      return;

    const auto before = m_examples.size();
    parse(body);
    if(m_examples.empty() && before == 0)
      return;

    const auto etag = reply->rawHeader("ETag");
    if(!etag.isEmpty())
      m_etag = QString::fromUtf8(etag);

#if defined(__EMSCRIPTEN__)
    // Not the filesystem: CacheLocation is backed by IndexedDB, whose writes
    // suspend, and this runs on a plain JS stack where suspending throws.
    QSettings s;
    s.setValue("score-docs/manifest", body);
    if(!etag.isEmpty())
      s.setValue("score-docs/etag", m_etag);
#else
    if(const auto folder = cacheFolder(); !folder.isEmpty())
    {
      QDir{}.mkpath(folder);
      if(QSaveFile f{folder + "/index.json"}; f.open(QIODevice::WriteOnly))
      {
        f.write(body);
        f.commit();
      }
      if(!etag.isEmpty())
      {
        if(QSaveFile e{folder + "/index.etag"}; e.open(QIODevice::WriteOnly))
        {
          e.write(etag);
          e.commit();
        }
      }
    }
#endif

    updated();
  });
}

QString OnlineExamples::write(const OnlineExample& ex, const QByteArray& data)
{
  const auto folder = installFolder(ex);
  if(folder.isEmpty() || data.isEmpty())
    return {};

  const auto writeFile = [](const QString& path, const QByteArray& bytes) {
    QDir{}.mkpath(QFileInfo{path}.absolutePath());
    QSaveFile f{path};
    return f.open(QIODevice::WriteOnly) && f.write(bytes) == bytes.size() && f.commit();
  };

  if(ex.format != QLatin1String{"zip"})
  {
    const auto path = folder + "." + ex.format;
    return writeFile(path, data) ? path : QString{};
  }

  QString score;
  for(const auto& [name, bytes] : zdl::unzip_all_files_to_memory(data))
  {
    // Entry names become paths under the library.
    if(name.isEmpty() || name.contains("..") || QDir::isAbsolutePath(name))
      continue;

    const auto path = folder + "/" + name;
    if(!writeFile(path, bytes))
      continue;

    if(score.isEmpty()
       && (name.endsWith(".score", Qt::CaseInsensitive)
           || name.endsWith(".scorejson", Qt::CaseInsensitive)))
      score = path;
  }
  return score;
}

void OnlineExamples::install(const OnlineExample& ex, std::function<void(QString)> done)
{
  if(installFolder(ex).isEmpty())
  {
    done({});
    return;
  }

  if(const auto existing = localPath(ex);
     !existing.isEmpty() && QFile::exists(existing))
  {
    done(existing);
    return;
  }

  auto reply = network().get(makeRequest(QUrl{ex.file}));
  connect(
      reply, &QNetworkReply::finished, this,
      [this, reply, ex, done = std::move(done)]() mutable {
    reply->deleteLater();
    QByteArray data;
    if(reply->error() == QNetworkReply::NoError)
      data = reply->readAll();

    // Writing suspends when the filesystem is backed by IndexedDB, which the
    // stack of a network callback does not allow; a queued call gets one that does.
    QTimer::singleShot(
        0, this, [ex, data = std::move(data), done = std::move(done)]() mutable {
      done(write(ex, data));
    });
  });
}
}
