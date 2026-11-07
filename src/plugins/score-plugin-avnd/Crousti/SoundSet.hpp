#pragma once
#include <Media/AudioArray.hpp>

#include <halp/sound_set.hpp>

#include <QDateTime>
#include <QFileSystemWatcher>
#include <QObject>
#include <QStringList>
#include <QTimer>

#include <score_plugin_avnd_export.h>

#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace oscr
{
struct SoundSetOptions
{
  QStringList extensions; //!< Lowercase, without the dot; empty = every audio file
  bool recursive{};
  halp::sound_decoding decoding{};
  int rate{};
};

//! A file of a sound set. Immutable once published, except for its pins and
//! request flag: the audio thread only reads it.
struct SoundSetEntry
{
  QString path;     //!< Absolute
  std::string name; //!< Relative to the folder
  QDateTime modified;
  qint64 size{};

  Media::audio_array data;
  std::vector<const float*> channels;
  int rate{};
  halp::sound_status status{};

  std::atomic<int32_t> pins{0};
  std::atomic<bool> requested{false};
};

//! The set as the processor sees it between two changes.
struct SoundSetSnapshot
{
  std::vector<std::shared_ptr<SoundSetEntry>> entries;
  std::vector<halp::sound_view> views;
  std::string folder;
  uint64_t generation{};

  //! Set by the audio thread once a newer snapshot replaced this one: from
  //! then on, only its pinned sounds can still be read.
  std::atomic<bool> superseded{false};

  void make_views();
  halp::sound_set_view view() const noexcept
  {
    return {views, folder, generation};
  }
};

/**
 * Lists, decodes and watches the folder of a sound_set port, and publishes the
 * result to the processor.
 *
 * Lives on the UI thread. Listing and decoding run on score::TaskPool. Each new
 * snapshot reaches the processor through `publish`, which swaps it in between
 * two ticks and flags the previous one as superseded. A superseded snapshot is
 * freed here, on the UI thread, once nothing pins the sounds only it holds.
 * The audio thread never frees nor drops a reference to anything.
 */
class SCORE_PLUGIN_AVND_EXPORT SoundSetLoader final : public QObject
{
public:
  //! Swaps `next` in, then flags `previous` as superseded, on the audio thread
  using publish_function = std::function<void(
      std::shared_ptr<SoundSetSnapshot> next, SoundSetSnapshot* previous)>;
  //! Hands what may still be read by the audio thread to a place where it is
  //! freed once the processor stopped running, e.g. an execution command
  using dispose_function
      = std::function<void(std::vector<std::shared_ptr<SoundSetSnapshot>>)>;

  SoundSetLoader(
      SoundSetOptions opts, publish_function publish, dispose_function dispose,
      QObject* parent);
  ~SoundSetLoader();

  //! A folder, or a file naming its folder; resolved
  void setPath(const QString& path);

  const std::shared_ptr<SoundSetSnapshot>& current() const noexcept { return m_current; }
  std::size_t retiredCount() const noexcept { return m_retired.size(); }

  //! Frees the superseded snapshots whose own sounds nobody pins anymore
  void reap();

private:
  void scheduleScan();
  void scan();
  void scanned(uint64_t token, std::shared_ptr<SoundSetSnapshot> snapshot);
  void decodeRequested();
  void decoded(uint64_t token, std::shared_ptr<SoundSetEntry> entry);
  void publishSnapshot(std::shared_ptr<SoundSetSnapshot> snapshot);
  void rewatch();

  SoundSetOptions m_opts;
  publish_function m_publish;
  dispose_function m_dispose;

  QString m_folder;
  uint64_t m_token{};
  uint64_t m_generation{};

  std::shared_ptr<SoundSetSnapshot> m_current;
  std::vector<std::shared_ptr<SoundSetSnapshot>> m_retired;
  std::vector<std::shared_ptr<SoundSetEntry>> m_decoding;

  QFileSystemWatcher m_watcher;
  QTimer m_rescan;
  QTimer m_reaper;
  QTimer m_requests;
};

//! The extensions a sound_set port lists, from its `extensions()` string
SCORE_PLUGIN_AVND_EXPORT QStringList soundSetExtensions(std::string_view exts);
}
