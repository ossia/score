#pragma once
#if defined(__APPLE__)
#include <Audio/AudioInterface.hpp>

#include <ossia/audio/miniaudio_protocol.hpp>
#if OSSIA_ENABLE_MINIAUDIO
#include <Audio/GenericMiniAudioInterface.hpp>
namespace Audio
{
class CoreAudioFactory final : public GenericMiniAudioFactory
{
  SCORE_CONCRETE("85115103-694a-4a3b-9274-76ef47aec5a9")
public:
  CoreAudioFactory() { rescan(); }
  ~CoreAudioFactory() override { }
  QString prettyName() const override { return QObject::tr("CoreAudio"); }
  bool available() const noexcept override { return true; }

  bool
  compareDeviceId(const ma_device_id& id, const QString& str) const noexcept override
  {
    return id.coreaudio == str;
  }
  bool
  compareDeviceId(const ma_device_id& id, std::string_view str) const noexcept override
  {
    return id.coreaudio == str;
  }
  void setDeviceId(ma_device_id& id, const QString& str) const noexcept override
  {
    auto u = str.toUtf8();
    std::fill_n(id.coreaudio, 256, 0);
    std::copy_n(u.data(), std::min((int)u.size(), 255), id.coreaudio);
  }

  QString deviceIdToString(const ma_device_id& id) const noexcept override
  {
    return QString::fromUtf8(id.coreaudio);
  }
};
}
#endif
#endif
