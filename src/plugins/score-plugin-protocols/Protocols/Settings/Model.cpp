#include <Protocols/Settings/Model.hpp>
namespace Protocols::Settings
{
#if defined(_WIN32)
  MidiAPI::operator QStringList() const { return {"MME", "UWP", "JACK"}; }
#elif defined(__APPLE__)
  MidiAPI::perator QStringList() const { return {"CoreMidi", "JACK"}; }
#elif defined(__linux__)
  MidiAPI::operator QStringList() const { return {"ALSA (sequencer)", "ALSA (raw)", "JACK"}; }
#elif defined(__emscripten__)
  MidiAPI::operator QStringList() const { return {"Emscripten"}; }
#else
  MidiAPI::operator QStringList() const { return {"No midi backend"}; }
#endif

namespace Parameters
{
SETTINGS_PARAMETER_IMPL(MidiAPI)
{
  QStringLiteral("Protocols/MidiAPI"), QStringList{Protocols::Settings::MidiAPI{}}[0]
};

static auto list()
{
  return std::tie(MidiAPI);
}
}

Model::Model(QSettings& set, const score::ApplicationContext& ctx)
{
  score::setupDefaultSettings(set, Parameters::list(), *this);
}

libremidi::API Model::getMidiApiAsEnum() const noexcept
{
  libremidi::API api = libremidi::API::UNSPECIFIED;
  if(m_MidiAPI == "MME") api = libremidi::API::WINDOWS_MM;
  else if(m_MidiAPI == "UWP") api = libremidi::API::WINDOWS_UWP;
  else if(m_MidiAPI == "CoreMidi") api = libremidi::API::MACOSX_CORE;
  else if(m_MidiAPI == "ALSA (sequencer)") api = libremidi::API::LINUX_ALSA_SEQ;
  else if(m_MidiAPI == "ALSA (raw)") api = libremidi::API::LINUX_ALSA_RAW;
  else if(m_MidiAPI == "JACK") api = libremidi::API::UNIX_JACK;
  else if(m_MidiAPI == "Emscripten") api = libremidi::API::EMSCRIPTEN_WEBMIDI;
  return api;
}

SCORE_SETTINGS_PARAMETER_CPP(QString, Model, MidiAPI)
}
