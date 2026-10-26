#pragma once
#if defined(__linux__)
#include <Audio/AudioInterface.hpp>

#include <ossia/audio/miniaudio_protocol.hpp>
#if OSSIA_ENABLE_MINIAUDIO
#include <Audio/GenericMiniAudioInterface.hpp>

#include <ossia/audio/libasound.hpp>

#include <cstdio>
#include <string>
namespace Audio
{
class ALSAMiniAudioFactory final : public GenericMiniAudioFactory
{
  SCORE_CONCRETE("e0c533da-a1f4-4795-90b5-a805cdfcb79f")
public:
  ALSAMiniAudioFactory() { }
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
    return compareDeviceId(id, std::string_view{str.toStdString()});
  }
  bool
  compareDeviceId(const ma_device_id& id, std::string_view str) const noexcept override
  {
    return id.alsa == str || id.alsa == stableName(str);
  }

  // Exclusive access opens "hw:", with a period of the buffer size; shared
  // access goes through dmix, whose period is used whatever the buffer size.
  ma_share_mode shareMode() const noexcept override { return ma_share_mode_exclusive; }

  void normalizeDeviceId(ma_device_id& id) const noexcept override
  {
    auto name = stableName(id.alsa);
    if(!name.empty() && name.size() < sizeof(id.alsa))
    {
      std::fill_n(id.alsa, sizeof(id.alsa), 0);
      std::copy_n(name.data(), name.size(), id.alsa);
    }
  }

  //! miniaudio's ":<card index>,<device>" as ":CARD=<card id>,DEV=<device>":
  //! the index changes when USB devices enumerate in another order. miniaudio
  //! puts "hw", "dmix" or "dsnoop" in front of ids starting with ':', which
  //! gives valid ALSA names with either form.
  static std::string stableName(std::string_view id)
  {
    int card = -1, dev = -1;
    if(id.size() < 4 || id[0] != ':' || id.find("CARD=") != std::string_view::npos
       || std::sscanf(std::string{id}.c_str(), ":%d,%d", &card, &dev) != 2)
      return {};

    auto& snd = ossia::libasound::instance();
    snd_ctl_t* ctl{};
    if(snd.ctl_open(&ctl, ("hw:" + std::to_string(card)).c_str(), SND_CTL_NONBLOCK) < 0)
      return {};
    snd_ctl_card_info_t* info{};
    snd_alloca(&info, snd, ctl_card_info);
    std::string res;
    if(snd.ctl_card_info(ctl, info) == 0)
      res = ":CARD=" + std::string{snd.ctl_card_info_get_id(info)}
            + ",DEV=" + std::to_string(dev);
    snd.ctl_close(ctl);
    return res;
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
