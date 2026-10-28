#pragma once
#include <Fx/Looper/AudioLayers.hpp>
#include <Fx/Looper/LooperView.hpp>

#include <score/document/DocumentContext.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <ossia/audio/write_loop_wav.hpp>

#include <boost/container/vector.hpp>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace Nodes::Looper
{
/**
 * Where a looper's takes go, and the names they get.
 *
 * A saved document keeps them next to it, in a "Takes" folder, which goes
 * along with the document when it is copied. An unsaved one keeps them in the
 * application's data folder, in a folder of its own; nothing deletes them
 * there, so a crash loses nothing.
 *
 * Every take gets a name of its own: a sound file is cached by its path, and
 * a take written over an earlier one would play the earlier one's audio.
 */
struct take_writer
{
  //! Takes in flight at once: a slot's path is written by the worker and read
  //! by the processing thread once the worker's result has reached it.
  static constexpr int slots = 4;

  QString folder;
  QString prefix;
  QString extension;
  std::array<std::string, slots> paths;
  std::array<std::string, slots> bases;
  //! Set on the main thread before any take is written. Called by a worker,
  //! the looper maybe gone: tells the looper's views what it left when it
  //! stopped.
  std::function<void(const view_message&)> tell_views;

  static QString folder_for(const score::DocumentContext& ctx)
  {
    const QString file = ctx.document.metadata().fileName();
    if(QFileInfo fi{file}; fi.isAbsolute() && fi.exists())
      return fi.absolutePath() + "/Takes";

    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + "/recordings/" + QString::number(ctx.document.id().val());
  }

  //! Main thread only.
  static std::shared_ptr<take_writer> make(
      const score::DocumentContext& ctx, std::string_view prefix,
      std::string_view extension)
  {
    auto w = std::make_shared<take_writer>();
    w->folder = folder_for(ctx);
    w->prefix = QString::fromUtf8(prefix.data(), qsizetype(prefix.size()));
    w->extension = QString::fromUtf8(extension.data(), qsizetype(extension.size()));
    for(auto& p : w->paths)
      p.reserve(1024);
    for(auto& p : w->bases)
      p.reserve(1024);
    return w;
  }

  QString base(int number) const
  {
    return QStringLiteral("%1/%2-take%3").arg(folder, prefix).arg(number);
  }
  QString file(int number, int layer) const
  {
    return layer < 0 ? QStringLiteral("%1.%2").arg(base(number), extension)
                     : QStringLiteral("%1-layer%2.%3").arg(base(number)).arg(layer + 1).arg(extension);
  }

  //! Worker thread: the file of a take, the mix when `layer` is -1, else one of
  //! its layers. A take has one number for all its files; 0 asks for the first
  //! number free in the folder, which is then set.
  const std::string& next_path(int slot, int& number, int layer)
  {
    // Loopers of a document write from several workers: the choice of a
    // number and the creation of its first file go together.
    static std::mutex choosing;
    std::lock_guard _{choosing};

    QDir dir{folder};
    QDir{}.mkpath(folder);
    if(number <= 0)
    {
      // A number is free when no file of the folder has it, whichever of a
      // take's files they are: the files of some passes alone have neither
      // the mix nor the first layer.
      const QString head = prefix + QStringLiteral("-take");
      std::vector<int> used;
      for(const QString& name :
          dir.entryList({head + QStringLiteral("*")}, QDir::Files | QDir::Hidden))
      {
        int end = int(head.size());
        while(end < name.size() && name[end].isDigit())
          end++;
        bool ok = false;
        const int n = name.mid(head.size(), end - head.size()).toInt(&ok);
        if(ok && end < name.size() && (name[end] == '.' || name[end] == '-'))
          used.push_back(n);
      }
      std::sort(used.begin(), used.end());
      number = 1;
      for(int n : used)
        if(n == number)
          number++;
        else if(n > number)
          break;
    }
    const QString path = file(number, layer);
    if(QFile f{path}; f.open(QIODevice::WriteOnly))
      f.close();
    paths[slot].assign(path.toStdString());
    bases[slot].assign(base(number).toStdString());
    return paths[slot];
  }

  //! The path of a take's files without the layer and the extension.
  const std::string& take_base(int slot) const noexcept { return bases[slot]; }
  const std::string& path(int slot) const noexcept { return paths[slot]; }
};

/**
 * The take the score's stop leaves: the mix and every pass of the loop, so
 * that any of them can still be placed once the looper is gone. A worker
 * writes it from what the looper handed over, which it owns, and then tells
 * the views with the looper's last view message, the take in it.
 */
struct stop_take_notice
{
  //! What the looper showed when it stopped, with its last take.
  view_message view{};
  //! The number the take gets among the looper's takes, the date it was
  //! recorded at and how it asks the timeline to follow the tempo.
  int take{};
  double date{};
  halp::clip_stretch stretch{};
  const char* extension{};

  //! Worker thread: the files of take `number` in `slot` are written, `mix`
  //! and `layers` of them, all of them when `ok`. The views are told either
  //! way: of the take, or of what the looper showed alone.
  void written(take_writer& w, int slot, bool ok, int layers, bool placeable)
  {
    if(ok)
    {
      view.set_take(w.take_base(slot), extension, true, layers, stretch, placeable);
      view.take = take;
      view.take_date = date;
      view.take_version = view.version;
    }
    if(w.tell_views)
      w.tell_views(view);
  }
};

//! Where a looper flattens a loop for a worker to write it. Shared with the
//! worker, so that a looper removed while its take is being written does not
//! take the audio away from under it.
struct export_buffer
{
  boost::container::vector<float> data;
  int channels{};
  int64_t capacity{};
  //! A job is writing it: the processing thread does not write it until the
  //! job is over. The job may outlive the looper that made it.
  std::atomic_bool in_use{};

  //! Main thread only. The memory is reserved without being written.
  static std::shared_ptr<export_buffer> make(int channels, int64_t frames)
  {
    auto b = std::make_shared<export_buffer>();
    b->channels = channels;
    b->capacity = frames;
    b->data.resize(std::size_t(channels) * std::size_t(frames), boost::container::default_init);
    return b;
  }

  float* channel(int c) noexcept { return data.data() + std::size_t(c) * std::size_t(capacity); }
};

//! What the audio looper hands over for the take its stop leaves: the
//! passes and the memory they are in, which the job flattens itself. Made on
//! the main thread; the processing thread fills it only while no job has it.
struct audio_stop_take
{
  audio_snapshot passes;
  stop_take_notice notice;
  std::atomic_bool in_use{};
};

//! A flattened audio loop to write, prepared on the processing thread and run
//! by a worker. The processing thread does not touch the buffer again until
//! the worker's result has come back.
struct take_job
{
  std::shared_ptr<take_writer> writer;
  std::shared_ptr<export_buffer> audio;
  //! The take the score's stop leaves: every file of it, written whatever
  //! the looper does meanwhile, from `stop` rather than from `audio`.
  std::shared_ptr<audio_stop_take> stop;
  // Kept small: a job is posted to the workers in fixed storage.
  int slot{};
  //! Number of the take, 0 for a new one; -1 for the mix, else the layer.
  int number{};
  int layer{-1};
  int channels{};
  int rate{};
  bool alone{};
  int64_t frames{};
  double tempo{};
  int meter_numerator{4};
  int meter_denominator{4};

  bool write(const float* const* data)
  {
    const auto& path = writer->next_path(slot, number, layer);
    const double quarters = tempo > 0 ? frames / double(rate) * tempo / 60. : 0.;
    ossia::wav_loop_info info;
    info.tempo = tempo;
    info.beats = int(std::lround(quarters));
    info.meter_numerator = meter_numerator;
    info.meter_denominator = meter_denominator;
    info.loop = true;
    return ossia::write_loop_wav(path, channels, rate, frames, data, info);
  }

  bool run()
  {
    if(stop)
      return run_stop();
    if(!audio)
      return false;
    struct release
    {
      export_buffer& b;
      ~release() { b.in_use = false; }
    } done{*audio};
    if(!writer || channels <= 0 || frames <= 0)
      return false;

    std::array<const float*, 16> data{};
    channels = std::min<int>({channels, audio->channels, int(data.size())});
    for(int c = 0; c < channels; c++)
      data[c] = audio->channel(c);
    return write(data.data());
  }

  //! The mix, then each pass, under one take number, flattened here. The
  //! pool is let go of here as well, so that the processing thread never
  //! frees it.
  bool run_stop()
  {
    auto& s = *stop;
    struct release
    {
      audio_stop_take& s;
      ~release()
      {
        s.passes.pool.reset();
        s.in_use = false;
      }
    } done{s};
    std::array<const float*, 16> data{};
    channels = std::min<int>({channels, s.passes.channels, int(data.size())});
    bool ok = writer && s.passes.pool && channels > 0 && frames > 0;
    if(ok)
    {
      std::vector<float> buffer(std::size_t(channels) * std::size_t(frames));
      const auto channel = [&](int c) { return buffer.data() + std::size_t(c) * frames; };
      for(int c = 0; c < channels; c++)
        data[c] = channel(c);
      number = 0;
      for(layer = -1; ok && layer < s.passes.count; layer++)
      {
        s.passes.flatten(channels, channel, layer);
        ok = write(data.data());
      }
    }
    if(writer)
      s.notice.written(*writer, slot, ok, s.passes.count, true);
    return ok;
  }
};
}
