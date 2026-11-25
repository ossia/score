#pragma once
#include <Gfx/Libav/LibavDevice.hpp>

#include <score_plugin_recording_export.h>

#include <QString>

#include <optional>
#include <vector>

namespace Recording
{
//! A way to encode video takes: the settings of the FFmpeg output device
struct VideoPreset
{
  QString name;
  Gfx::LibavSettings settings;
};

//! The presets score comes with: H.264, H.265, HAP, ProRes 422, FFV1
SCORE_PLUGIN_RECORDING_EXPORT
std::vector<VideoPreset> builtinVideoPresets();

//! What a take is encoded with when nothing was chosen: the first preset
SCORE_PLUGIN_RECORDING_EXPORT
Gfx::LibavSettings defaultVideoEncoding();

//! The settings as the FFmpeg device saves them, and back
SCORE_PLUGIN_RECORDING_EXPORT
QString videoEncodingToJson(const Gfx::LibavSettings& s);
SCORE_PLUGIN_RECORDING_EXPORT
std::optional<Gfx::LibavSettings> videoEncodingFromJson(const QString& json);

//! The file extension of a muxer ("matroska" gives "mkv")
SCORE_PLUGIN_RECORDING_EXPORT
QString extensionOfMuxer(const QString& muxer);
}
