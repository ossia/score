#include <Audio/Settings/Model.hpp>
namespace Audio::Settings
{
namespace Parameters
{
// portaudio is the default
SETTINGS_PARAMETER_IMPL(Driver)
{
  QStringLiteral("Audio/Driver"),
#if defined(_WIN32)
      Audio::AudioFactory::ConcreteKey
  {
    score::uuids::string_generator::compute(
        "afcd9c64-0367-4fa1-b2bb-ee65b1c5e5a7")
  } // WASAPI
#elif defined(__APPLE__)
      Audio::AudioFactory::ConcreteKey
  {
    score::uuids::string_generator::compute(
        "e7543875-3b22-457c-bf41-75504637686f")
  }
#elif defined(__linux__)
      Audio::AudioFactory::ConcreteKey
  {
    score::uuids::string_generator::compute(
        "3533ee88-9a8d-486c-b20b-6c966cf4eaa0")
  } // ALSA
#else
      Audio::AudioFactory::ConcreteKey
  {
    score::uuids::string_generator::compute(
        "e7543875-3b22-457c-bf41-75504637686f")
  }
#endif
};
SETTINGS_PARAMETER_IMPL(CardIn){QStringLiteral("Audio/CardIn"), ""};
SETTINGS_PARAMETER_IMPL(CardOut){QStringLiteral("Audio/CardOut"), ""};
SETTINGS_PARAMETER_IMPL(BufferSize){QStringLiteral("Audio/BufferSize"), 64};
SETTINGS_PARAMETER_IMPL(Rate){QStringLiteral("Audio/SamplingRate"), 44100};
SETTINGS_PARAMETER_IMPL(DefaultIn){QStringLiteral("Audio/DefaultIn"), 2};
SETTINGS_PARAMETER_IMPL(DefaultOut){QStringLiteral("Audio/DefaultOut"), 2};

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
  Type ModelType::get##Name() const { return m_##Name; }             \
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

Audio::AudioFactory::ConcreteKey Model::getDriver() const
{
  return m_Driver;
}

void Model::setDriver(Audio::AudioFactory::ConcreteKey val)
{
  // Reset to default in case of invalid parameters.
  auto& factories = score::AppContext().interfaces<AudioFactoryList>();
  if (factories.find(val) == factories.end())
  {
    val = Parameters::Driver.def;
  }

  if (val == m_Driver)
    return;

  m_Driver = val;

  QSettings s;
  s.setValue(Parameters::Driver.key, QVariant::fromValue(m_Driver));
}

AUDIO_PARAMETER_CPP(QString, Model, CardIn)
AUDIO_PARAMETER_CPP(QString, Model, CardOut)
AUDIO_PARAMETER_CPP(int, Model, BufferSize)
AUDIO_PARAMETER_CPP(int, Model, Rate)
AUDIO_PARAMETER_CPP(int, Model, DefaultIn)
AUDIO_PARAMETER_CPP(int, Model, DefaultOut)
}
