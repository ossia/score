#pragma once
#include <QObject>
#include <QString>

#include <score_lib_base_export.h>

#include <functional>
#include <map>
#include <vector>
#include <verdigris>

namespace score
{
//! One entry of assets/scores/index.json.
struct SCORE_LIB_BASE_EXPORT OnlineExample
{
  QString id;       //!< e.g. "examples/3d/sponza"
  QString name;
  QString description;
  QString section;  //!< "examples", "reference"...
  QString group;    //!< Folder within the section
  QString category;
  QString page;     //!< Documentation page, may be empty
  QString file;     //!< The .score or .zip to download
  QString format;   //!< "score" or "zip"
  QString image;
  qint64 size{};
  //! Hash of what the manifest says about the file to download, so that a new
  //! version is noticed whatever describes it (size, checksum, date...).
  QString fingerprint;
};

//! Reads, caches and refreshes the documentation site's manifest.
class SCORE_LIB_BASE_EXPORT OnlineExamples : public QObject
{
  W_OBJECT(OnlineExamples)

public:
  explicit OnlineExamples(QObject* parent = nullptr);
  ~OnlineExamples() override;

  static QString manifestUrl();
  static QString installRoot();

  const std::vector<OnlineExample>& examples() const noexcept { return m_examples; }

  //! Synchronous, no network.
  void loadCache();

  //! Emits updated() only if the manifest changed.
  void refresh();

  //! Empty when the id is not a relative path of plain folder names, which
  //! could otherwise name the library's own folders.
  static QString installFolder(const OnlineExample& ex);

  //! `folder`, or the only folder in it: where the files of a zip example are
  static QString contentFolder(const QString& folder);

  //! Empty until the example has been installed.
  static QString localPath(const OnlineExample& ex);

  //! The example is installed, and the manifest describes another file than
  //! the one that was downloaded. Unknown, hence false, for copies downloaded
  //! without the record this compares to.
  static bool changedOnline(const OnlineExample& ex);

  //! Calls @p done with the local path, or an empty string on failure.
  //! An installed example is downloaded again only when @p replace is set; the
  //! local copy is then replaced whole, files the new version does not have
  //! included, and only once the new version is completely written: on any
  //! failure the local copy stays as it was.
  //! A request for an example already being downloaded waits for that download.
  void install(
      const OnlineExample& ex, std::function<void(QString)> done, bool replace = false);

  bool isDownloading(const OnlineExample& ex) const;

  void updated() E_SIGNAL(SCORE_LIB_BASE_EXPORT, updated)

private:
  void refresh(bool conditional);
  static QString cacheFolder();
  void parse(const QByteArray& json);
  static QString write(const OnlineExample& ex, const QByteArray& data);

  std::vector<OnlineExample> m_examples;
  //! Per example id, who waits for its download in flight.
  std::map<QString, std::vector<std::function<void(QString)>>> m_downloads;
  QString m_etag;
};
}
