#include <Gfx/Libav/LibavDevice.hpp>

namespace Gfx
{

namespace
{
struct LibavPresetEnumerator : public Device::DeviceEnumerator
{
  std::vector<std::pair<QString, LibavSettings>> presets;

  void enumerate(std::function<void(const QString&, const Device::DeviceSettings&)> f)
      const override
  {
    for(auto& [label, set] : presets)
    {
      Device::DeviceSettings s;
      s.name = "FFmpeg";
      s.protocol = LibavProtocolFactory::static_concreteKey();
      s.deviceSpecificSettings = QVariant::fromValue(set);
      f(label, s);
    }
  }
};

// Helper lambdas for building presets
auto makeInputAdder(LibavPresetEnumerator* e)
{
  return [e](const char* name, const char* path,
             ossia::hash_map<QString, QString> opts = {}) {
    LibavSettings s;
    s.direction = LibavSettings::Input;
    s.path = path;
    s.options = std::move(opts);
    e->presets.emplace_back(QString::fromUtf8(name), std::move(s));
  };
}

auto makeOutputAdder(LibavPresetEnumerator* e)
{
  return [e](const char* name, const char* path, const char* muxer, const char* venc,
             const char* aenc, const char* pixfmt, const char* smpfmt,
             ossia::hash_map<QString, QString> opts = {}) {
    LibavSettings s;
    s.direction = LibavSettings::Output;
    s.path = path;
    s.width = 1280;
    s.height = 720;
    s.rate = 30;
    s.audio_channels = 2;
    s.muxer = muxer;
    s.video_encoder_short = venc;
    s.audio_encoder_short = aenc;
    s.video_render_pixfmt = "rgba";
    s.video_converted_pixfmt = pixfmt;
    s.audio_converted_smpfmt = smpfmt;
    s.options = std::move(opts);
    e->presets.emplace_back(QString::fromUtf8(name), std::move(s));
  };
}

void addInputEnumerators(Device::DeviceEnumerators& enums)
{
  // Test sources (lavfi virtual inputs — no hardware needed)
  {
    auto* e = new LibavPresetEnumerator;
    auto add = makeInputAdder(e);
    add("Test: SMPTE color bars", "smptebars=size=1280x720:rate=30",
        {{"format", "lavfi"}});
    add("Test: testsrc2", "testsrc2=size=1280x720:rate=30", {{"format", "lavfi"}});
    add("Test: solid color", "color=c=red:size=1280x720:rate=30",
        {{"format", "lavfi"}});
    add("Test: mandelbrot", "mandelbrot=size=1280x720:rate=30",
        {{"format", "lavfi"}});
    enums.push_back({"Test Sources", e});
  }

  // Network input
  {
    auto* e = new LibavPresetEnumerator;
    auto add = makeInputAdder(e);
    add("RTSP stream", "rtsp://192.168.1.1:554/stream", {});
    add("SRT stream", "srt://192.168.1.1:40052?mode=caller", {});
    enums.push_back({"Stream In", e});
  }

  // File / sequence input
  {
    auto* e = new LibavPresetEnumerator;
    auto add = makeInputAdder(e);
    add("Video file", "<PROJECT>:/video.mp4", {});
    add("Video file (loop)", "<PROJECT>:/video.mp4", {{"loop", "-1"}});
    add("Image sequence", "<PROJECT>:/frame_%05d.png",
        {{"format", "image2"}, {"framerate", "30"}, {"start_number", "0"}});
    add("Image sequence (loop)", "<PROJECT>:/frame_%05d.png",
        {{"format", "image2"}, {"framerate", "30"}, {"start_number", "0"}, {"loop", "-1"}});
    enums.push_back({"File In", e});
  }

  // Device / capture input
  {
    auto* e = new LibavPresetEnumerator;
    auto add = makeInputAdder(e);
#if defined(__linux__)
    add("V4L2 camera", "/dev/video0",
        {{"format", "v4l2"},
         {"input_format", "mjpeg"},
         {"framerate", "30"},
         {"video_size", "1920x1080"}});
    add("Screen capture (X11)", ":0.0",
        {{"format", "x11grab"}, {"framerate", "30"}, {"video_size", "1920x1080"}});
    add("Screen region (X11)", ":0.0+100,200",
        {{"format", "x11grab"}, {"framerate", "30"}, {"video_size", "640x480"}});
#elif defined(_WIN32)
    add("Screen capture", "desktop", {{"format", "gdigrab"}, {"framerate", "30"}});
    add("Window capture", "title=Window Title",
        {{"format", "gdigrab"}, {"framerate", "30"}});
#elif defined(__APPLE__)
    add("Screen capture", "Capture screen 0:none",
        {{"format", "avfoundation"}, {"framerate", "30"}, {"pixel_format", "uyvy422"}});
    add("Camera capture", "0:none",
        {{"format", "avfoundation"}, {"framerate", "30"}, {"pixel_format", "uyvy422"}});
#endif
    if(!e->presets.empty())
      enums.push_back({"Device In", e});
    else
      delete e;
  }
}

void addOutputEnumerators(Device::DeviceEnumerators& enums)
{
  // Video file recording
  {
    auto* e = new LibavPresetEnumerator;
    auto add = makeOutputAdder(e);
    add("MP4 H.264", "<PROJECT>:/main.mp4", "mp4", "libx264", "", "yuv420p", "",
        {{"preset", "medium"}, {"crf", "23"}, {"bf", "3"}});
    add("MP4 H.264 + AAC", "<PROJECT>:/main.mp4", "mp4", "libx264", "aac", "yuv420p",
        "fltp", {{"preset", "medium"}, {"crf", "23"}, {"bf", "3"}});
    add("MKV H.265", "<PROJECT>:/main.mkv", "matroska", "libx265", "", "yuv420p", "",
        {{"preset", "medium"}, {"crf", "28"}, {"bf", "3"}});
    add("MKV FFV1 lossless", "<PROJECT>:/main.mkv", "matroska", "ffv1", "", "yuv444p",
        "", {});
    add("WebM VP9", "<PROJECT>:/main.webm", "webm", "libvpx-vp9", "", "yuv420p", "",
        {{"crf", "30"}, {"b:v", "0"}, {"row-mt", "1"}});
    add("MOV ProRes 422", "<PROJECT>:/main.mov", "mov", "prores_ks", "", "yuv422p10le",
        "", {{"profile", "2"}}); // profile 2 = ProRes 422
    add("MOV ProRes 4444", "<PROJECT>:/main.mov", "mov", "prores_ks", "", "yuva444p10le",
        "", {{"profile", "4"}}); // profile 4 = ProRes 4444
    add("MXF DNxHD 185", "<PROJECT>:/main.mxf", "mxf", "dnxhd", "", "yuv422p", "",
        {{"b:v", "185M"}});
    add("MP4 AV1 (SVT)", "<PROJECT>:/main.mp4", "mp4", "libsvtav1", "", "yuv420p", "",
        {{"crf", "30"}, {"preset", "6"}});
    enums.push_back({"Record Video", e});
  }

  // Audio file recording
  {
    auto* e = new LibavPresetEnumerator;
    auto add = makeOutputAdder(e);
    add("WAV 16-bit", "<PROJECT>:/main.wav", "wav", "", "pcm_s16le", "", "s16", {});
    add("WAV 24-bit", "<PROJECT>:/main.wav", "wav", "", "pcm_s24le", "", "s32", {});
    add("WAV 32-bit float", "<PROJECT>:/main.wav", "wav", "", "pcm_f32le", "", "flt",
        {});
    add("FLAC", "<PROJECT>:/main.flac", "flac", "", "flac", "", "s16", {});
    add("Ogg Opus", "<PROJECT>:/main.opus", "ogg", "", "libopus", "", "flt",
        {{"b:a", "128k"}});
    add("MP3", "<PROJECT>:/main.mp3", "mp3", "", "libmp3lame", "", "s16",
        {{"q:a", "2"}});
    enums.push_back({"Record Audio", e});
  }

  // Image sequence output
  {
    auto* e = new LibavPresetEnumerator;
    auto add = makeOutputAdder(e);
    add("PNG sequence", "<PROJECT>:/frame_%05d.png", "image2", "png", "", "rgba", "",
        {});
    add("JPEG sequence", "<PROJECT>:/frame_%05d.jpg", "image2", "mjpeg", "", "yuv420p",
        "", {{"q:v", "2"}, {"color_range", "pc"}});
    add("EXR sequence", "<PROJECT>:/frame_%05d.exr", "image2", "exr", "", "gbrpf32le",
        "", {});
    add("TIFF sequence", "<PROJECT>:/frame_%05d.tiff", "image2", "tiff", "", "rgba", "",
        {});
    add("DPX sequence", "<PROJECT>:/frame_%05d.dpx", "image2", "dpx", "", "rgb48le", "",
        {});
    enums.push_back({"Record Image Sequence", e});
  }

  // Network streaming output
  {
    auto* e = new LibavPresetEnumerator;
    auto add = makeOutputAdder(e);
    add("UDP MJPEG", "udp://192.168.1.80:8081", "mjpeg", "mjpeg", "", "yuv420p", "",
        {{"fflags", "+nobuffer+genpts"},
         {"flags", "+low_delay"},
         {"flush_packets", "1"},
         {"bf", "0"},
         {"color_range", "pc"}});
    // clang-format off
    add("SRT H.264",
        "srt://:40052?mode=listener&latency=2000&transtype=live", "mpegts",
        "libx264", "", "yuv420p", "",
        {{"preset", "ultrafast"},
         {"tune", "zerolatency"},
         {"flush_packets", "1"}});
    add("SRT HEVC",
        "srt://:40052?mode=listener&latency=2000&transtype=live", "mpegts",
        "libx265", "", "yuv420p", "",
        {{"preset", "ultrafast"},
         {"tune", "zerolatency"},
         {"flush_packets", "1"}});
    // clang-format on
    add("RTMP H.264", "rtmp://localhost/live/stream", "flv", "libx264", "aac", "yuv420p",
        "fltp", {{"preset", "veryfast"}, {"tune", "zerolatency"}, {"g", "60"}});
    add("HLS H.264", "<PROJECT>:/stream.m3u8", "hls", "libx264", "", "yuv420p", "",
        {{"preset", "veryfast"},
         {"tune", "zerolatency"},
         {"hls_time", "2"},
         {"hls_list_size", "5"},
         {"hls_flags", "delete_segments"}});
    add("DASH H.264", "<PROJECT>:/stream.mpd", "dash", "libx264", "", "yuv420p", "",
        {{"preset", "veryfast"},
         {"tune", "zerolatency"},
         {"seg_duration", "2"},
         {"window_size", "5"}});
    add("WHIP (WebRTC)", "http://localhost:8080/whip", "whip", "libx264", "", "yuv420p",
        "", {{"preset", "ultrafast"}, {"tune", "zerolatency"}});
    add("RTP MPEG-TS (multicast)", "rtp://239.0.0.1:5004", "rtp_mpegts", "libx264", "",
        "yuv420p", "", {{"preset", "ultrafast"}, {"tune", "zerolatency"}, {"ttl", "4"}});
    add("SAP/SDP announce", "sap://239.255.255.255", "sap", "libx264", "", "yuv420p", "",
        {{"preset", "ultrafast"}, {"tune", "zerolatency"}});
    enums.push_back({"Stream Out", e});
  }

  // Hardware-accelerated output
  {
    auto* e = new LibavPresetEnumerator;
    auto add = makeOutputAdder(e);
    add("MP4 H.264 (NVENC)", "<PROJECT>:/main.mp4", "mp4", "h264_nvenc", "", "yuv420p",
        "", {{"preset", "p4"}, {"rc", "constqp"}, {"qp", "23"}});
    add("MP4 HEVC (NVENC)", "<PROJECT>:/main.mp4", "mp4", "hevc_nvenc", "", "yuv420p",
        "", {{"preset", "p4"}, {"rc", "constqp"}, {"qp", "28"}});
    add("MP4 AV1 (NVENC)", "<PROJECT>:/main.mp4", "mp4", "av1_nvenc", "", "yuv420p", {},
        {{"preset", "p4"}, {"rc", "constqp"}, {"qp", "30"}});
    add("MP4 H.264 (VAAPI)", "<PROJECT>:/main.mp4", "mp4", "h264_vaapi", "", "nv12", "",
        {{"qp", "23"}});
    add("MP4 H.264 (QSV)", "<PROJECT>:/main.mp4", "mp4", "h264_qsv", "", "nv12", "",
        {{"global_quality", "23"}});
    add("MP4 H.264 (Vulkan)", "<PROJECT>:/main.mp4", "mp4", "h264_vulkan", "", "yuv420p",
        {}, {});
    enums.push_back({"HW Accelerated", e});
  }

  // Device outputs
  {
    auto* e = new LibavPresetEnumerator;
    auto addOut = makeOutputAdder(e);

    // JACK audio output (cross-platform — FFmpeg has libjack input, output via protocol)
#if defined(__linux__)
    // V4L2 virtual webcam (Linux only — requires v4l2loopback kernel module)
    addOut(
        "V4L2 virtual webcam", "/dev/video10", "v4l2", "rawvideo", "", "yuv420p", "",
        {});

    // Linux audio outputs
    addOut("ALSA audio output", "default", "alsa", "", "pcm_s16le", "", "s16", {});
    addOut("PulseAudio output", "default", "pulse", "", "pcm_s16le", "", "s16", {});

    // Framebuffer (Linux only — direct console output)
    addOut("Framebuffer output", "/dev/fb0", "fbdev", "rawvideo", "", "bgra", "", {});

    // XVideo display (Linux X11)
    addOut("XVideo display", ":0.0", "xv", "rawvideo", "", "yuv420p", "", {});
#endif

#if defined(__APPLE__)
    // macOS audio output
    addOut(
        "AudioToolbox output", "default", "audiotoolbox", "", "pcm_s16le", "", "s16",
        {});
#endif

    if(!e->presets.empty())
      enums.push_back({"Device Out", e});
    else
      delete e;
  }
}

}
Device::DeviceEnumerators
LibavProtocolFactory::getEnumerators(const score::DocumentContext& ctx) const
{
  Device::DeviceEnumerators enums;
  addInputEnumerators(enums);
  addOutputEnumerators(enums);
  return enums;
}
}