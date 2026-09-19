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
#include <QStringList>
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
  req.setRawHeader("User-Agent", "ossia-score");
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
  const auto folder = cacheFolder();
  if(folder.isEmpty())
    return;

  QFile f{folder + "/index.json"};
  if(f.open(QIODevice::ReadOnly))
    parse(f.readAll());

  QFile e{folder + "/index.etag"};
  if(e.open(QIODevice::ReadOnly))
    m_etag = QString::fromUtf8(e.readAll()).trimmed();
}

void OnlineExamples::refresh()
{
  auto req = makeRequest(QUrl{manifestUrl()});
  if(!m_etag.isEmpty())
    req.setRawHeader("If-None-Match", m_etag.toUtf8());

  auto reply = network().get(req);
  connect(reply, &QNetworkReply::finished, this, [this, reply] {
    reply->deleteLater();
    if(reply->error() != QNetworkReply::NoError)
      return;

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

    if(const auto folder = cacheFolder(); !folder.isEmpty())
    {
      QDir{}.mkpath(folder);
      if(QSaveFile f{folder + "/index.json"}; f.open(QIODevice::WriteOnly))
      {
        f.write(body);
        f.commit();
      }
      const auto etag = reply->rawHeader("ETag");
      if(!etag.isEmpty())
      {
        m_etag = QString::fromUtf8(etag);
        if(QSaveFile e{folder + "/index.etag"}; e.open(QIODevice::WriteOnly))
        {
          e.write(etag);
          e.commit();
        }
      }
    }

    updated();
  });
}

void OnlineExamples::install(const OnlineExample& ex, std::function<void(QString)> done)
{
  const auto folder = installFolder(ex);
  if(folder.isEmpty())
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

  if(ex.format == QLatin1String{"zip"})
  {
    // Not openArchive(): that asks the user where to extract.
    zdl::download_and_extract(
        QUrl{ex.file}, folder,
        [done](const std::vector<QString>& files) {
      auto it = std::find_if(files.begin(), files.end(), [](const QString& f) {
        return f.endsWith(".score", Qt::CaseInsensitive)
               || f.endsWith(".scorejson", Qt::CaseInsensitive);
      });
      done(it != files.end() ? *it : QString{});
    },
        [](qint64, qint64) {},
        [done, id = ex.id](const QString& err) {
      qWarning() << "online example" << id << ":" << err;
      done({});
    });
    return;
  }

  const auto path = folder + "." + ex.format;
  auto reply = network().get(makeRequest(QUrl{ex.file}));
  connect(
      reply, &QNetworkReply::finished, this,
      [reply, path, done = std::move(done)]() mutable {
    reply->deleteLater();
    if(reply->error() != QNetworkReply::NoError)
    {
      done({});
      return;
    }

    const auto data = reply->readAll();
    if(data.isEmpty())
    {
      done({});
      return;
    }

    QDir{}.mkpath(QFileInfo{path}.absolutePath());
    QSaveFile f{path};
    if(!f.open(QIODevice::WriteOnly) || f.write(data) != data.size() || !f.commit())
    {
      done({});
      return;
    }

    done(path);
  });
}
}
