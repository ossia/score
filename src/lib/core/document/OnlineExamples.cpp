#include "OnlineExamples.hpp"

#include <score/tools/Platforms.hpp>

#include <core/document/DocumentTemplates.hpp>

#include <QCryptographicHash>
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
#include <set>

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

// The id becomes a path under the library: only folder names that mean the
// same on every platform, none of which is "." or "..".
bool isSafeId(const QString& id)
{
  if(id.isEmpty())
    return false;

  for(const auto part : QStringView{id}.tokenize(u'/', Qt::KeepEmptyParts))
  {
    if(part.isEmpty() || part == u"." || part == u"..")
      return false;
    // Windows drops trailing dots and spaces: "a." and "a" are the same folder.
    if(part.endsWith(u'.') || part.endsWith(u' '))
      return false;
    for(const QChar c : part)
    {
      if(c.unicode() < 0x20)
        return false;
      switch(c.unicode())
      {
        case '\\':
        case ':':
        case '*':
        case '?':
        case '"':
        case '<':
        case '>':
        case '|':
          return false;
        default:
          break;
      }
    }
  }
  return true;
}

// What identifies the file to download: the entry without what only presents
// the example. QJsonObject keeps its keys sorted, which makes the text stable.
QString fingerprint(QJsonObject entry)
{
  for(const char* key :
      {"name", "description", "section", "group", "category", "page", "image",
       "platforms"})
    entry.remove(QLatin1String{key});
  return QStringLiteral("sha256:")
         + QString::fromLatin1(
             QCryptographicHash::hash(
                 QJsonDocument{entry}.toJson(QJsonDocument::Compact),
                 QCryptographicHash::Sha256)
                 .toHex());
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
  if(root.isEmpty() || !isSafeId(ex.id))
    return {};

  const auto cleanRoot = QDir::cleanPath(root);
  auto folder = QDir::cleanPath(cleanRoot + "/" + ex.id);
  if(!folder.startsWith(cleanRoot + "/"))
    return {};
  return folder;
}

QString OnlineExamples::contentFolder(const QString& folder)
{
  // The site publishes each zip example with its files in a folder of the
  // example's name
  const QDir dir{folder};
  const auto entries = dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
  if(entries.size() == 1)
    if(const QFileInfo only{dir.filePath(entries.front())}; only.isDir())
      return only.absoluteFilePath();
  return folder;
}

QString OnlineExamples::localPath(const OnlineExample& ex)
{
  const auto folder = installFolder(ex);
  if(folder.isEmpty())
    return {};

  if(ex.format != QLatin1String{"zip"})
  {
    auto path = folder + "." + ex.format;
    return QFile::exists(path) ? path : QString{};
  }

  // Copies installed with the archive's own folder kept have the score one
  // level down
  const QDir dir{contentFolder(folder)};
  const auto scores = dir.entryList({"*.score", "*.scorejson"}, QDir::Files, QDir::Name);
  return scores.isEmpty() ? QString{} : dir.filePath(scores.front());
}

namespace
{
// What was downloaded is recorded in the settings rather than next to the copy:
// the library folder is the user's, and every file in it shows in the library.
QString downloadRecordKey(const OnlineExample& ex)
{
  return QStringLiteral("score-docs/downloaded/") + ex.id;
}
}

bool OnlineExamples::changedOnline(const OnlineExample& ex)
{
  const auto recorded = QSettings{}.value(downloadRecordKey(ex)).toString();
  if(recorded.isEmpty() || localPath(ex).isEmpty())
    return false;

  if(recorded.startsWith(QLatin1String{"sha256:"}))
    return recorded != ex.fingerprint;

  // Copies downloaded before fingerprints were recorded have their size.
  bool ok{};
  const auto size = recorded.toLongLong(&ok);
  return ok && ex.size > 0 && size != ex.size;
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
    if(ex.file.isEmpty() || !isSafeId(ex.id))
      continue;
    // Also the extension of the copy.
    if(ex.format != QLatin1String{"score"} && ex.format != QLatin1String{"scorejson"}
       && ex.format != QLatin1String{"zip"})
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
    ex.fingerprint = fingerprint(e);
    out.push_back(std::move(ex));
  }

  // A zip example is replaced as a whole folder: one whose folder holds
  // another example would take that one with it.
  std::set<QString> ids;
  for(const auto& ex : out)
    ids.insert(ex.id);
  std::erase_if(out, [&](const OnlineExample& ex) {
    const auto prefix = ex.id + '/';
    const auto it = ids.lower_bound(prefix);
    return it != ids.end() && it->startsWith(prefix);
  });

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

  // Extracted next to the copy, then swapped with it: until the new version is
  // completely written the copy stays as it was, and stays on any failure.
  const QFileInfo target{folder};
  const auto parent = target.absolutePath();
  const auto incoming = parent + "/." + target.fileName() + ".incoming";
  const auto previous = parent + "/." + target.fileName() + ".previous";
  // Left over by an interrupted install.
  QDir{incoming}.removeRecursively();
  QDir{previous}.removeRecursively();

  const auto fail = [&] {
    QDir{incoming}.removeRecursively();
    return QString{};
  };

  if(!QDir{}.mkpath(incoming))
    return fail();

  // Refuses an archive with entries that would land outside of the folder.
  QString error;
  const auto files = zdl::unzip_all_files_to_folder(data, incoming, error);
  if(files.empty() || !error.isEmpty())
    return fail();

  // What goes in place of the copy: the archive's own folder when it has one
  const auto content = contentFolder(incoming);
  const auto scores
      = QDir{content}.entryList({"*.score", "*.scorejson"}, QDir::Files, QDir::Name);
  if(scores.isEmpty())
    return fail();

  const bool hadCopy = target.exists();
  if(hadCopy && !QDir{}.rename(folder, previous))
    return fail();

  if(!QDir{}.rename(content, folder))
  {
    if(hadCopy)
      QDir{}.rename(previous, folder);
    return fail();
  }
  QDir{incoming}.removeRecursively();

  if(hadCopy)
    QDir{previous}.removeRecursively();
  return QDir{folder}.filePath(scores.front());
}

bool OnlineExamples::isDownloading(const OnlineExample& ex) const
{
  return m_downloads.contains(ex.id);
}

void OnlineExamples::install(
    const OnlineExample& ex, std::function<void(QString)> done, bool replace)
{
  if(installFolder(ex).isEmpty())
  {
    done({});
    return;
  }

  if(auto it = m_downloads.find(ex.id); it != m_downloads.end())
  {
    it->second.push_back(std::move(done));
    return;
  }

  if(const auto existing = localPath(ex); !existing.isEmpty() && !replace)
  {
    done(existing);
    return;
  }

  m_downloads[ex.id].push_back(std::move(done));

  auto reply = network().get(makeRequest(QUrl{ex.file}));
  connect(reply, &QNetworkReply::finished, this, [this, reply, ex] {
    reply->deleteLater();
    QByteArray data;
    if(reply->error() == QNetworkReply::NoError)
      data = reply->readAll();

    // Writing suspends when the filesystem is backed by IndexedDB, which the
    // stack of a network callback does not allow; a queued call gets one that does.
    QTimer::singleShot(0, this, [this, ex, data = std::move(data)] {
      auto path = write(ex, data);
      if(!path.isEmpty())
        QSettings{}.setValue(downloadRecordKey(ex), ex.fingerprint);

      // A callback may destroy this object.
      auto node = m_downloads.extract(ex.id);
      if(node.empty())
        return;
      for(auto& done : node.mapped())
        done(path);
    });
  });
}
}
