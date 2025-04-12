#pragma once
#if defined(__linux__)
#include <Audio/AudioInterface.hpp>

#include <ossia/audio/miniaudio_protocol.hpp>
#if OSSIA_ENABLE_MINIAUDIO
#include <Audio/GenericMiniAudioInterface.hpp>

#include <ossia/audio/libasound.hpp>
namespace Audio
{
class ALSAMiniAudioFactory final : public GenericMiniAudioFactory
{
  SCORE_CONCRETE("e0c533da-a1f4-4795-90b5-a805cdfcb79f")
public:
  ALSAMiniAudioFactory() { rescan(); }
  ~ALSAMiniAudioFactory() override { }
  QString prettyName() const override { return QObject::tr("ALSA (MiniAudio)"); }
  bool available() const noexcept override
  {
    try
    {
      ossia::libasound::instance();
      return true;
    }
    catch(...)
    {
      return false;
    }
  }

  bool
  compareDeviceId(const ma_device_id& id, const QString& str) const noexcept override
  {
    return id.alsa == str;
  }
  bool
  compareDeviceId(const ma_device_id& id, std::string_view str) const noexcept override
  {
    return id.alsa == str;
  }
  void setDeviceId(ma_device_id& id, const QString& str) const noexcept override
  {
    auto u = str.toUtf8();
    std::fill_n(id.alsa, 256, 0);
    std::copy_n(u.data(), std::min((int)u.size(), 255), id.alsa);
  }

  QString deviceIdToString(const ma_device_id& id) const noexcept override
  {
    return QString::fromUtf8(id.alsa);
  }
};
}
#endif
#endif
