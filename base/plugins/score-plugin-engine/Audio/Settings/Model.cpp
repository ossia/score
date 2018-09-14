#include <Audio/Settings/Model.hpp>
namespace Audio::Settings
{
namespace Parameters
{
// portaudio is the default
SETTINGS_PARAMETER_IMPL(Driver){
    QStringLiteral("Audio/Driver"),
    Audio::AudioFactory::ConcreteKey{score::uuids::string_generator::compute(
        "e7543875-3b22-457c-bf41-75504637686f")}};
SETTINGS_PARAMETER_IMPL(CardIn){QStringLiteral("Audio/CardIn"), ""};
SETTINGS_PARAMETER_IMPL(CardOut){QStringLiteral("Audio/CardOut"), ""};
SETTINGS_PARAMETER_IMPL(BufferSize){QStringLiteral("Audio/BufferSize"), 64};
SETTINGS_PARAMETER_IMPL(Rate){QStringLiteral("Audio/SamplingRate"), 44100};
SETTINGS_PARAMETER_IMPL(DefaultIn){QStringLiteral("Audio/DefaultIn"), 8};
SETTINGS_PARAMETER_IMPL(DefaultOut){QStringLiteral("Audio/DefaultOut"), 8};

static auto list()
{
  return std::tie(
      Driver, Rate, CardIn, CardOut, BufferSize, DefaultIn, DefaultOut);
}
}

Model::Model(QSettings& set, const score::ApplicationContext& ctx)
{
  score::setupDefaultSettings(set, Parameters::list(), *this);
}

// For the audio settings, we only want to send a "changed" signal when
// everything has been updated
#define AUDIO_PARAMETER_CPP(Type, ModelType, Name)                   \
  Type ModelType::get##Name() const                                  \
  {                                                                  \
    return m_##Name;                                                 \
  }                                                                  \
                                                                     \
  void ModelType::set##Name(Type val)                                \
  {                                                                  \
    if (val == m_##Name)                                             \
      return;                                                        \
                                                                     \
    m_##Name = val;                                                  \
                                                                     \
    QSettings s;                                                     \
    s.setValue(Parameters::Name.key, QVariant::fromValue(m_##Name)); \
  }

AUDIO_PARAMETER_CPP(Audio::AudioFactory::ConcreteKey, Model, Driver)
AUDIO_PARAMETER_CPP(QString, Model, CardIn)
AUDIO_PARAMETER_CPP(QString, Model, CardOut)
AUDIO_PARAMETER_CPP(int, Model, BufferSize)
AUDIO_PARAMETER_CPP(int, Model, Rate)
AUDIO_PARAMETER_CPP(int, Model, DefaultIn)
AUDIO_PARAMETER_CPP(int, Model, DefaultOut)
}
