#pragma once
#include <QObject>
#include <QString>

#include <score_lib_base_export.h>

#include <functional>
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

  static QString installFolder(const OnlineExample& ex);

  //! Empty until the example has been installed.
  static QString localPath(const OnlineExample& ex);

  //! Calls @p done with the local path, or an empty string on failure.
  void install(const OnlineExample& ex, std::function<void(QString)> done);

  void updated() W_SIGNAL(updated)

private:
  void refresh(bool conditional);
  static QString cacheFolder();
  void parse(const QByteArray& json);
  static QString write(const OnlineExample& ex, const QByteArray& data);

  std::vector<OnlineExample> m_examples;
  QString m_etag;
};
}
