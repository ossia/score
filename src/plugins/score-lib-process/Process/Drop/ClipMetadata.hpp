#pragma once
#include <halp/custom_widgets.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace Process
{
/**
 * What a clip dragged or sent to the timeline says of how its files are to
 * be played, carried in a drop next to the files' URLs, under its own mime
 * type: a line per file, its absolute path then tab-separated key=value
 * pairs. A drop without it, or a file it does not name, is made as any
 * dropped file is.
 *
 *   /path/to/Looper-take3.wav\tstretch=timestretch\n
 */
inline constexpr std::string_view clip_metadata_mime = "application/x-ossia-take";

struct clip_file_metadata
{
  std::string path;
  halp::clip_stretch stretch{halp::clip_stretch::automatic};
};

inline constexpr std::string_view clip_stretch_names[]{
    "automatic",   "none",       "repitch", "repitch_fast",
    "timestretch", "timestretch_percussive"};

inline std::string_view to_string(halp::clip_stretch s) noexcept
{
  const auto i = std::size_t(s);
  return i < std::size(clip_stretch_names) ? clip_stretch_names[i] : clip_stretch_names[0];
}

inline halp::clip_stretch clip_stretch_from_string(std::string_view s) noexcept
{
  for(std::size_t i = 0; i < std::size(clip_stretch_names); i++)
    if(clip_stretch_names[i] == s)
      return halp::clip_stretch(i);
  return halp::clip_stretch::automatic;
}

//! Empty when there is nothing to say: every file is left to the host.
inline std::string
encode_clip_metadata(const std::vector<std::string>& files, halp::clip_stretch stretch)
{
  std::string res;
  if(stretch == halp::clip_stretch::automatic)
    return res;
  for(const auto& f : files)
  {
    res += f;
    res += "\tstretch=";
    res += to_string(stretch);
    res += '\n';
  }
  return res;
}

inline std::vector<clip_file_metadata> decode_clip_metadata(std::string_view data)
{
  std::vector<clip_file_metadata> res;
  while(!data.empty())
  {
    auto eol = data.find('\n');
    std::string_view line = data.substr(0, eol);
    data = eol == std::string_view::npos ? std::string_view{} : data.substr(eol + 1);
    if(!line.empty() && line.back() == '\r')
      line.remove_suffix(1);

    auto tab = line.find('\t');
    clip_file_metadata m;
    m.path = std::string{line.substr(0, tab)};
    if(m.path.empty())
      continue;
    while(tab != std::string_view::npos)
    {
      line.remove_prefix(tab + 1);
      tab = line.find('\t');
      const auto field = line.substr(0, tab);
      const auto eq = field.find('=');
      if(eq == std::string_view::npos)
        continue;
      if(field.substr(0, eq) == "stretch")
        m.stretch = clip_stretch_from_string(field.substr(eq + 1));
    }
    res.push_back(std::move(m));
  }
  return res;
}
}
