#pragma once
#include <QImage>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>

#include <score_plugin_recording_export.h>

#include <ossia/gfx/port_index.hpp>

#include <atomic>
#include <functional>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

namespace score
{
struct DocumentContext;
}
namespace Process
{
class Port;
}
namespace Gfx
{
class TexturePreview;
struct LibavEncoder;
}

namespace Recording
{
/**
 * Records what a texture outlet produces to a video file.
 *
 * The frames are read back through the preview path of the graphics context
 * (Gfx::GfxContext::add_preview), which reads the port back from the render
 * list that already draws it, so nothing is rendered twice. They are taken on
 * the GUI thread at the take's frame rate, repeated or skipped to keep to the
 * time that passes, and encoded on a thread of their own; the render thread
 * and the audio thread wait for nothing.
 */
class SCORE_PLUGIN_RECORDING_EXPORT TextureRecorder final : public QObject
{
public:
  //! The texture ports the recorder can record: outlets, and inlets (what
  //! their cable or their device brings)
  static bool canRecord(const Process::Port& port) noexcept;
  //! What renders the texture of a port now; {-1, -1} for nothing
  static ossia::gfx::port_index
  producerOf(const Process::Port& port, const score::DocumentContext& doc);

  //! Where the frames come from, asked on each frame: a producer can be made
  //! again while playing
  using Producer = std::function<ossia::gfx::port_index()>;

  //! What a texture outlet produces
  TextureRecorder(
      const Process::Port& port, const score::DocumentContext& ctx, QString file,
      int fps = 30, bool lossless = false);
  //! Any producer: a camera or another texture input of a device
  TextureRecorder(
      Producer producer, const score::DocumentContext& ctx, QString file, int fps = 30,
      bool lossless = false);
  ~TextureRecorder() override;

  bool start();
  //! Stops taking frames, and waits for the file to be written
  void stop();

  //! Frames written; 0 until the first one could be read back
  int64_t frames() const noexcept { return m_written.load(); }
  bool started() const noexcept { return m_frameSize.isValid(); }
  const QString& file() const noexcept { return m_file; }
  int fps() const noexcept { return m_fps; }

private:
  void take();
  void encodeLoop();

  Producer m_producer;
  const score::DocumentContext& m_ctx;
  QString m_file;
  int m_fps{30};
  bool m_lossless{};
  std::unique_ptr<Gfx::TexturePreview> m_preview;
  QTimer m_timer;
  QSize m_frameSize;
  int64_t m_taken{};
  qint64 m_startMs{};

  std::thread m_thread;
  std::mutex m_lock;
  std::condition_variable m_cond;
  std::deque<QImage> m_queue;
  bool m_done{};
  std::atomic<int64_t> m_written{0};
};
}
