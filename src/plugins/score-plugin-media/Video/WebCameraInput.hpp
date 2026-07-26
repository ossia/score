#pragma once
#include <Media/Libav.hpp>

#if defined(__EMSCRIPTEN__) && SCORE_HAS_LIBAV
#include <Video/ExternalInput.hpp>
#include <Video/FrameQueue.hpp>

#include <score_plugin_media_export.h>

#include <string>
#include <vector>

namespace Video
{
struct WebCameraDevice
{
  std::string id;
  std::string label;
};

//! Starts an asynchronous navigator.mediaDevices.enumerateDevices()
SCORE_PLUGIN_MEDIA_EXPORT void webCameraScan() noexcept;

//! Incremented every time a scan completes ; the device list is only valid once
//! this changed at least once.
SCORE_PLUGIN_MEDIA_EXPORT int webCameraScanGeneration() noexcept;

//! Devices seen by the last completed scan. Ids and labels are empty until the
//! user has granted camera permission at least once.
SCORE_PLUGIN_MEDIA_EXPORT std::vector<WebCameraDevice> webCameraDevices() noexcept;

/**
 * @brief Camera capture through getUserMedia + WebCodecs.
 *
 * Everything browser-side happens on the main thread: a JS promise chain pumps
 * VideoFrames into a slot, which dequeue_frame() copies out. The gfx graph calls
 * dequeue_frame() from that same thread on wasm.
 */
class SCORE_PLUGIN_MEDIA_EXPORT WebCameraInput final : public ExternalInput
{
public:
  WebCameraInput() noexcept;
  ~WebCameraInput() noexcept override;

  bool load(const std::string& deviceId, int w, int h, double fps) noexcept;

  bool start() noexcept override;
  void stop() noexcept override;

  AVFrame* dequeue_frame() noexcept override;
  void release_frame(AVFrame* frame) noexcept override;

private:
  void fetch() noexcept;

  FrameQueue m_frames;
  std::string m_deviceId;
  int m_requestedWidth{};
  int m_requestedHeight{};
  double m_requestedFps{};
  int m_handle{};
  bool m_reportedError{};
};
}
#endif
