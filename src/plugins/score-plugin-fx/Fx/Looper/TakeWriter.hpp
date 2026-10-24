#pragma once
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
#include <cmath>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

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
//! What a take is written as: the loop as it is heard, or each of its passes
//! in a file of its own, or both.
enum class TakeFiles
{
  Mix,
  Layers,
  Both
};

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

    QDir{}.mkpath(folder);
    if(number <= 0)
    {
      for(int n = 1; n < 100000; n++)
      {
        if(!QFileInfo::exists(file(n, -1)) && !QFileInfo::exists(file(n, 0)))
        {
          number = n;
          break;
        }
      }
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

//! Where a looper flattens a loop for a worker to write it. Shared with the
//! worker, so that a looper removed while its take is being written does not
//! take the audio away from under it.
struct export_buffer
{
  boost::container::vector<float> data;
  int channels{};
  int64_t capacity{};

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

//! A flattened audio loop to write, prepared on the processing thread and run
//! by a worker. The processing thread does not touch the buffer again until
//! the worker's result has come back.
struct take_job
{
  std::shared_ptr<take_writer> writer;
  std::shared_ptr<export_buffer> audio;
  int slot{};
  //! Number of the take, 0 for a new one; -1 for the mix, else the layer.
  int number{};
  int layer{-1};
  int channels{};
  int64_t frames{};
  int rate{};
  double tempo{};
  int meter_numerator{4};
  int meter_denominator{4};

  bool run()
  {
    if(!writer || !audio || channels <= 0 || frames <= 0)
      return false;

    std::array<const float*, 16> data{};
    channels = std::min<int>({channels, audio->channels, int(data.size())});
    for(int c = 0; c < channels; c++)
      data[c] = audio->channel(c);

    const auto& path = writer->next_path(slot, number, layer);
    const double quarters = tempo > 0 ? frames / double(rate) * tempo / 60. : 0.;
    ossia::wav_loop_info info;
    info.tempo = tempo;
    info.beats = int(std::lround(quarters));
    info.meter_numerator = meter_numerator;
    info.meter_denominator = meter_denominator;
    info.loop = true;
    return ossia::write_loop_wav(path, channels, rate, frames, data.data(), info);
  }
};
}
