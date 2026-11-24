#pragma once
#include <Recording/Session/RecordOptions.hpp>

#include <ossia/gfx/port_index.hpp>

#include <QObject>
#include <QPointer>
#include <QSize>
#include <QString>
#include <QTimer>

#include <score_plugin_recording_export.h>

#include <functional>
#include <memory>

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
struct LibavEncoder;
}

namespace Recording
{
/**
 * Records what a texture produces to a video file.
 *
 * An encoder output (Gfx::LibavEncoderNode, the node the FFmpeg output device
 * renders to) is added to the graphics graph for the take, fed by the
 * producer as a preview node is: nothing changes in the document. It renders
 * the producer at the take's size and frame rate on the render thread,
 * converts it to YUV on the GPU when the codec wants it, and encodes it there.
 */
class SCORE_PLUGIN_RECORDING_EXPORT TextureRecorder final : public QObject
{
public:
  //! Where the frames come from, asked again while recording: a producer can
  //! be made again while playing
  using Producer = std::function<ossia::gfx::port_index()>;

  //! The texture ports the recorder can record: outlets, and inlets (what
  //! their cable or their device brings)
  static bool canRecord(const Process::Port& port) noexcept;
  //! What renders the texture of a port now; {-1, -1} for nothing
  static ossia::gfx::port_index
  producerOf(const Process::Port& port, const score::DocumentContext& doc);

  TextureRecorder(
      const Process::Port& port, const score::DocumentContext& ctx, QString file,
      VideoSettings settings);
  TextureRecorder(
      Producer producer, const score::DocumentContext& ctx, QString file,
      VideoSettings settings);
  ~TextureRecorder() override;

  bool start();
  //! Takes the encoder out of the graph and finishes the file
  void stop();

  //! Frames encoded so far
  int64_t frames() const noexcept;
  const QString& file() const noexcept { return m_file; }
  const VideoSettings& settings() const noexcept { return m_settings; }

private:
  void follow();

  Producer m_producer;
  const score::DocumentContext& m_ctx;
  QString m_file;
  VideoSettings m_settings;

  std::shared_ptr<Gfx::LibavEncoder> m_encoder;
  int32_t m_node{-1};
  ossia::gfx::port_index m_source{-1, -1};
  QTimer m_follow;
  int64_t m_frames{};
};
}
