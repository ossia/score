#include "VideoEncoding.hpp"

#include <Device/Protocol/DeviceSettings.hpp>

#include <score/serialization/JSONVisitor.hpp>

namespace Recording
{
namespace
{
Gfx::LibavSettings preset(
    const char* muxer, const char* encoder, const char* pixfmt,
    ossia::hash_map<QString, QString> options)
{
  Gfx::LibavSettings s;
  s.direction = Gfx::LibavSettings::Output;
  s.width = 1920;
  s.height = 1080;
  s.rate = 30;
  s.audio_channels = 0;
  s.muxer = muxer;
  s.video_encoder_short = encoder;
  s.video_render_pixfmt = "rgba";
  s.video_converted_pixfmt = pixfmt;
  s.options = std::move(options);
  return s;
}
}

std::vector<VideoPreset> builtinVideoPresets()
{
  return {
      {QObject::tr("H.264 (MKV)"),
       preset("matroska", "libx264", "yuv420p", {{"preset", "veryfast"}, {"crf", "18"}})},
      {QObject::tr("H.265 (MKV)"),
       preset("matroska", "libx265", "yuv420p", {{"preset", "fast"}, {"crf", "22"}})},
      {QObject::tr("HAP (MOV)"), preset("mov", "hap", "rgba", {{"format", "hap"}})},
      {QObject::tr("ProRes 422 (MOV)"),
       preset("mov", "prores_ks", "yuv422p10le", {{"profile", "2"}})},
      {QObject::tr("FFV1, lossless (MKV)"), preset("matroska", "ffv1", "yuv420p", {})}};
}

Gfx::LibavSettings defaultVideoEncoding()
{
  return builtinVideoPresets().front().settings;
}

QString videoEncodingToJson(const Gfx::LibavSettings& s)
{
  Device::DeviceSettings ds;
  ds.name = QStringLiteral("take");
  ds.protocol = Gfx::LibavProtocolFactory::static_concreteKey();
  ds.deviceSpecificSettings = QVariant::fromValue(s);
  JSONReader r;
  r.readFrom(ds);
  return QString::fromUtf8(r.toByteArray());
}

std::optional<Gfx::LibavSettings> videoEncodingFromJson(const QString& json)
{
  if(json.isEmpty())
    return std::nullopt;
  const auto doc = readJson(json.toUtf8());
  if(!doc.IsObject())
    return std::nullopt;
  JSONWriter w{doc};
  Device::DeviceSettings ds;
  w.writeTo(ds);
  if(!ds.deviceSpecificSettings.canConvert<Gfx::LibavSettings>())
    return std::nullopt;
  auto s = ds.deviceSpecificSettings.value<Gfx::LibavSettings>();
  s.direction = Gfx::LibavSettings::Output;
  return s;
}

QString extensionOfMuxer(const QString& muxer)
{
  if(muxer == "matroska")
    return QStringLiteral("mkv");
  if(muxer == "mpegts")
    return QStringLiteral("ts");
  if(muxer.isEmpty())
    return QStringLiteral("mkv");
  // mov, mp4, webm, avi, nut...: named as their extension
  return muxer.section(',', 0, 0);
}
}
