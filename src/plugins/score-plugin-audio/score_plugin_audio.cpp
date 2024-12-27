// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include "score_plugin_audio.hpp"

#include <Audio/ALSAInterface.hpp>
#include <Audio/ALSAPortAudioInterface.hpp>
#include <Audio/ASIOPortAudioInterface.hpp>
#include <Audio/AudioApplicationPlugin.hpp>
#include <Audio/AudioDevice.hpp>
#include <Audio/AudioPreviewExecutor.hpp>
#include <Audio/CoreAudioInterface.hpp>
#include <Audio/CoreAudioPortAudioInterface.hpp>
#include <Audio/DummyInterface.hpp>
#include <Audio/GenericPortAudioInterface.hpp>
#include <Audio/JackInterface.hpp>
#include <Audio/MMEPortAudioInterface.hpp>
#include <Audio/PipeWireInterface.hpp>
#include <Audio/PortAudioInterface.hpp>
#include <Audio/SDLInterface.hpp>
#include <Audio/Settings/Factory.hpp>
#include <Audio/WASAPIPortAudioInterface.hpp>
#include <Audio/WDMKSPortAudioInterface.hpp>

#include <score/plugins/FactorySetup.hpp>
#include <score/plugins/InterfaceList.hpp>
#include <score/plugins/StringFactoryKey.hpp>
#include <score/plugins/UuidKeySerialization.hpp>

#include <wobjectimpl.h>

#include <ossia-config.hpp>

#if OSSIA_AUDIO_PORTAUDIO && defined(__linux__) && __has_include(<alsa/asoundlib.h>)
#include <alsa/asoundlib.h>
static void asound_error(
    const char* file, int line, const char* function, int err, const char* fmt, ...)
{
}
#endif

score_plugin_audio::score_plugin_audio()
{
  qRegisterMetaType<Audio::AudioFactory::ConcreteKey>(
      "Audio::AudioFactory::ConcreteKey");
  qRegisterMetaType<Audio::Settings::ExternalTransport>(
      "Audio::Settings::ExternalTransport");

  auto only_dummy_audio = qEnvironmentVariableIntValue("SCORE_ONLY_DUMMY_AUDIO") > 0;

  if(!only_dummy_audio)
  {
#if OSSIA_AUDIO_JACK
    jack_set_error_function(
        [](const char* str) { std::cerr << "JACK ERROR: " << str << std::endl; });
    jack_set_info_function([](const char* str) {
      // std::cerr << "JACK INFO: " << str << std::endl;
    });
#endif
#if OSSIA_AUDIO_PORTAUDIO && defined(__linux__) && __has_include(<alsa/asoundlib.h>)
    auto asound = dlopen("libasound.so.2", RTLD_LAZY | RTLD_LOCAL);
    auto sym = (decltype(snd_lib_error_set_handler)*)dlsym(
        asound, "snd_lib_error_set_handler");
    sym(asound_error);
#endif
  }
}

score_plugin_audio::~score_plugin_audio() { }

score::GUIApplicationPlugin*
score_plugin_audio::make_guiApplicationPlugin(const score::GUIApplicationContext& app)
{
  return new Audio::ApplicationPlugin{app};
}

std::vector<std::unique_ptr<score::InterfaceListBase>>
score_plugin_audio::factoryFamilies()
{
  return make_ptr_vector<score::InterfaceListBase, Audio::AudioFactoryList>();
}

template <typename... Args>
auto add_factories(
    std::vector<score::InterfaceBase*>& vec, const auto& ctx,
    const score::InterfaceKey& key) noexcept
{
  ossia::for_each_type_if_tagged<Args...>([&](auto t) {
    using fw_t = typename decltype(t)::type;
    return fw_t{}(ctx, key, vec);
  });
}

std::vector<score::InterfaceBase*> score_plugin_audio::factories(
    const score::ApplicationContext& ctx, const score::InterfaceKey& key) const
{
  std::vector<score::InterfaceBase*> vec;
  {
    // Common stuff
    add_factories<
        FW<Device::ProtocolFactory
#if defined(OSSIA_PROTOCOL_AUDIO)
           ,
           Dataflow::AudioProtocolFactory
#endif
           >,
        FW<score::SettingsDelegateFactory, Audio::Settings::Factory>,
        FW<Execution::ExecutionAction, Audio::AudioPreviewExecutor>>(vec, ctx, key);
  }

  using namespace Audio;
  auto only_dummy_audio = qEnvironmentVariableIntValue("SCORE_ONLY_DUMMY_AUDIO") > 0;
  auto forced_backend = qEnvironmentVariable("SCORE_AUDIO_BACKEND").toLower();
  if(only_dummy_audio)
  {
    add_factories<FW<Audio::AudioFactory, Audio::DummyFactory>>(vec, ctx, key);
    return vec;
  }
  else if(!forced_backend.isEmpty())
  {
    if(forced_backend == "sdl")
    {
#if defined(OSSIA_AUDIO_SDL)
      add_factories<FW<Audio::AudioFactory, Audio::SDLFactory>>(vec, ctx, key);
      return vec;
#endif
    }
    else if(forced_backend == "jack")
    {
#if defined(OSSIA_AUDIO_JACK)
      add_factories<FW<Audio::AudioFactory, Audio::JackFactory>>(vec, ctx, key);
      return vec;
#endif
    }
    else if(forced_backend == "pipewire")
    {
#if defined(OSSIA_AUDIO_PIPEWIRE)
      add_factories<FW<Audio::AudioFactory, Audio::PipeWireAudioFactory>>(vec, ctx, key);
      return vec;
#endif
    }
    else if(forced_backend == "alsa")
    {
#if OSSIA_AUDIO_ALSA
      add_factories<FW<Audio::AudioFactory, Audio::ALSAFactory>>(vec, ctx, key);
      return vec;
#endif
    }
    else if(forced_backend == "dummy")
    {
      add_factories<FW<Audio::AudioFactory, Audio::DummyFactory>>(vec, ctx, key);
      return vec;
    }
  }

  add_factories<
      FW<Audio::AudioFactory, Audio::DummyFactory
#if defined(OSSIA_AUDIO_JACK)
         ,
         Audio::JackFactory
#endif
// #if defined(OSSIA_AUDIO_PULSEAUDIO)
//          ,
//          Audio::PulseAudioFactory
// #endif
#if OSSIA_AUDIO_ALSA
         ,
         Audio::ALSAFactory
#endif
#if defined(OSSIA_AUDIO_PORTAUDIO)
#if __has_include(<pa_asio.h>)
         ,
         Audio::ASIOFactory
#endif
//#if __has_include(<pa_win_wdmks.h>)
//         ,
//         Audio::WDMKSFactory
//#endif
#if __has_include(<pa_win_wasapi.h>)
         ,
         Audio::WASAPIFactory
#endif
//#if __has_include(<pa_win_wmme.h>)
//         ,
//         Audio::MMEFactory
//#endif
#if __has_include(<pa_linux_alsa.h>)
         ,
         Audio::ALSAPortAudioFactory
#endif
#if __has_include(<pa_mac_core.h>)
         ,
         Audio::CoreAudioPortAudioFactory
#endif
#if !__has_include(<pa_asio.h>) && \
    !__has_include(<pa_win_wdmks.h>) && \
    !__has_include(<pa_win_wasapi.h>) && \
    !__has_include(<pa_win_wmme.h>) && \
    !__has_include(<pa_linux_alsa.h>) && \
    !__has_include(<pa_mac_core.h>)
         ,
         Audio::PortAudioFactory
#endif
#endif
#if defined(OSSIA_AUDIO_SDL)
         ,
         Audio::SDLFactory
#endif
#if defined(OSSIA_AUDIO_PIPEWIRE)
         ,
         Audio::PipeWireAudioFactory
#endif
#if defined(OSSIA_AUDIO_MINIAUDIO)
         ,
         Audio::CoreAudioFactory
#endif
         >>(vec, ctx, key);

  return vec;
}

auto score_plugin_audio::required() const -> std::vector<score::PluginKey>
{
  return {};
}

#include <score/plugins/PluginInstances.hpp>
SCORE_EXPORT_PLUGIN(score_plugin_audio)
