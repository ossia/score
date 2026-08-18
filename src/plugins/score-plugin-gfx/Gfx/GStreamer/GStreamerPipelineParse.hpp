#pragma once

/**
 * @file GStreamerPipelineParse.hpp
 * @brief Appsink discovery and media-type classification from a pipeline string.
 *
 * A live source has no negotiated caps when the pipeline is built, so the only
 * thing that can say whether an appsink carries audio or video -- and at what
 * geometry -- is the pipeline description itself. Kept out of GStreamerDevice.cpp
 * so it can be exercised without a GStreamer library: this is ctre over a string
 * plus Video::gstreamerToLibav().
 */

#include <Video/GStreamerCompatibility.hpp>

#include <ossia/detail/parse_strict.hpp>

extern "C" {
#include <libavutil/pixfmt.h>
}

#include <ctre.hpp>

#include <algorithm>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace Gfx::GStreamer
{

struct AppsinkInfo
{
  std::string name;
  bool is_video{};
  int width{};
  int height{};
  AVPixelFormat pixfmt{AV_PIX_FMT_NONE};
  int channels{};
  int rate{};
  std::function<void(int, int, AVPixelFormat)> on_format_change;
};

inline constexpr auto appsink_name_rexp
    = ctll::fixed_string{R"(appsink\b[^!]*?\bname\s*=\s*([A-Za-z0-9_]+))"};
inline constexpr auto elem_name_rexp
    = ctll::fixed_string{R"(\bname\s*=\s*([A-Za-z0-9_]+))"};

inline std::vector<std::string> find_appsink_names(const std::string& pipeline)
{
  std::vector<std::string> names;
  for(auto m : ctre::search_all<appsink_name_rexp>(pipeline))
    names.push_back(m.get<1>().to_string());
  return names;
}

// Find all name=<identifier> patterns in the pipeline string
// (covers any element, not just appsink/appsrc)
inline std::vector<std::string> find_all_named_elements(const std::string& pipeline)
{
  std::vector<std::string> names;
  for(auto m : ctre::search_all<elem_name_rexp>(pipeline))
    names.push_back(m.get<1>().to_string());
  return names;
}

inline constexpr auto channels_rexp
    = ctll::fixed_string{R"(channels=(?:\(int\))?\s*([0-9]+))"};
inline constexpr auto rate_rexp
    = ctll::fixed_string{R"(rate=(?:\(int\))?\s*([0-9]+))"};
inline constexpr auto width_rexp
    = ctll::fixed_string{R"(width=(?:\(int\))?\s*([0-9]+))"};
inline constexpr auto height_rexp
    = ctll::fixed_string{R"(height=(?:\(int\))?\s*([0-9]+))"};
inline constexpr auto format_rexp
    = ctll::fixed_string{R"(format=(?:\(string\))?\s*([A-Za-z0-9]+))"};

template <ctll::fixed_string Rexp>
inline int last_int_of(std::string_view str, int fallback)
{
  int ret = fallback;
  for(auto m : ctre::search_all<Rexp>(str))
    if(auto v = ossia::parse_strict<int>(m.template get<1>().to_view()))
      ret = *v;
  return ret;
}

// Guess an appsink's media type from the pipeline string when caps are
// not negotiated yet: look for the last audio/video-ish token occurring
// before "appsink ... name=<sink_name>".
inline void classify_from_pipeline_string(
    const std::string& pipeline, const std::string& sink_name, AppsinkInfo& info)
{
  std::size_t sink_pos = std::string::npos;
  for(auto m : ctre::search_all<appsink_name_rexp>(pipeline))
  {
    if(m.get<1>().to_view() == sink_name)
    {
      sink_pos = std::distance(pipeline.begin(), m.get<0>().begin());
      break;
    }
  }
  const std::string_view before
      = std::string_view{pipeline}.substr(0, sink_pos);

  static constexpr std::string_view audio_tokens[]
      = {"audio/x-raw", "audioconvert", "audioresample", "audiotestsrc"};
  static constexpr std::string_view video_tokens[]
      = {"video/x-raw", "videoconvert", "videoscale", "videotestsrc", "videorate"};

  std::size_t last_audio = std::string::npos, last_video = std::string::npos;
  for(auto tok : audio_tokens)
    if(auto p = before.rfind(tok); p != std::string::npos)
      last_audio = (last_audio == std::string::npos) ? p : std::max(last_audio, p);
  for(auto tok : video_tokens)
    if(auto p = before.rfind(tok); p != std::string::npos)
      last_video = (last_video == std::string::npos) ? p : std::max(last_video, p);

  const bool is_audio = last_audio != std::string::npos
                        && (last_video == std::string::npos || last_audio > last_video);
  if(is_audio)
  {
    info.is_video = false;
    info.channels = last_int_of<channels_rexp>(before, 2);
    info.rate = last_int_of<rate_rexp>(before, 48000);
  }
  else
  {
    info.is_video = true;
    info.width = last_int_of<width_rexp>(before, 640);
    info.height = last_int_of<height_rexp>(before, 480);
    info.pixfmt = AV_PIX_FMT_RGBA;
    std::string format;
    for(auto m : ctre::search_all<format_rexp>(before))
      format = m.get<1>().to_string();
    if(!format.empty())
    {
      auto& map = ::Video::gstreamerToLibav();
      if(auto it = map.find(format); it != map.end())
        info.pixfmt = it->second;
    }
  }
}

}
