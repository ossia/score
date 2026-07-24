#pragma once
#if defined(_WIN32)
#include <Audio/AudioInterface.hpp>

#include <ossia/audio/miniaudio_protocol.hpp>
#if OSSIA_ENABLE_MINIAUDIO
#include <Audio/GenericMiniAudioInterface.hpp>

#include <algorithm>

namespace Audio
{
class WASAPIMiniAudioFactory final : public GenericMiniAudioFactory
{
  SCORE_CONCRETE("2838b52b-9c0c-466e-a2b0-8c6d8cb8fc6d")
public:
  WASAPIMiniAudioFactory() { rescan(); }
  ~WASAPIMiniAudioFactory() override { }
  QString prettyName() const override { return QObject::tr("WASAPI (miniaudio)"); }
  bool available() const noexcept override { return true; }

  // Expose the "Default device (follow system)" entry: when selected, the
  // engine opens the OS default device with a NULL miniaudio device id, so
  // WASAPI reroutes automatically when the Windows mixer default changes.
  bool followsDefaultDevice() const noexcept override { return true; }

  // WASAPI identifies devices with a wchar_t string (ma_device_id::wasapi).
  bool
  compareDeviceId(const ma_device_id& id, const QString& str) const noexcept override
  {
    return QString::fromWCharArray(id.wasapi) == str;
  }
  bool
  compareDeviceId(const ma_device_id& id, std::string_view str) const noexcept override
  {
    return QString::fromWCharArray(id.wasapi)
           == QString::fromUtf8(str.data(), (int)str.size());
  }
  void setDeviceId(ma_device_id& id, const QString& str) const noexcept override
  {
    std::fill_n(id.wasapi, 64, (ma_wchar_win32)0);
    int n = (int)str.size();
    if(n > 63)
      n = 63;
    str.left(n).toWCharArray(id.wasapi);
    id.wasapi[n] = 0;
  }

  QString deviceIdToString(const ma_device_id& id) const noexcept override
  {
    return QString::fromWCharArray(id.wasapi);
  }
};
}
#endif
#endif
