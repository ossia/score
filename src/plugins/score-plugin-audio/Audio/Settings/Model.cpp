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
        "d98fca36-4e50-4802-a825-2fa213f95265")
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
        "015cb998-7bad-46e0-ba57-669b7733eadc")
  } // PulseAudio
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
SETTINGS_PARAMETER_IMPL(AutoStereo){QStringLiteral("Audio/AutoStereo"), true};

static auto list()
{
  return std::tie(
      Driver, Rate, CardIn, CardOut, BufferSize, DefaultIn, DefaultOut, AutoStereo);
}
}

Model::Model(QSettings& set, const score::ApplicationContext& ctx)
{
  score::setupDefaultSettings(set, Parameters::list(), *this);
}

Audio::AudioFactory::ConcreteKey Model::getDriver() const
{
  return m_Driver;
}

void Model::setDriver(Audio::AudioFactory::ConcreteKey val)
{
  // Reset to default in case of invalid parameters.
  auto& factories = score::AppContext().interfaces<AudioFactoryList>();
  if (!factories.get(val))
  {
    val = Parameters::Driver.def;
  }

  if (val == m_Driver)
    return;

  m_Driver = val;

  QSettings s;
  s.setValue(Parameters::Driver.key, QVariant::fromValue(m_Driver));
  // Special case for dummy driver: set reasonable values
  if(m_Driver == Audio::AudioFactory::ConcreteKey{score::uuids::string_generator::compute("13dabcc3-9cda-422f-a8c7-5fef5c220677")})
  {
    m_Rate = 44100;
    s.setValue(Parameters::Rate.key, QVariant::fromValue(m_Rate));

    m_BufferSize = 1024;
    s.setValue(Parameters::BufferSize.key, QVariant::fromValue(m_BufferSize));
  }
}

SCORE_SETTINGS_PARAMETER_CPP(QString, Model, CardIn)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, CardOut)

int Model::getRate() const { return m_Rate; }

void Model::setRate(int val)
{
  if (val <= 0)
    val = 44100;
  if (val == m_Rate)
    return;

  m_Rate = val;

  QSettings s;
  s.setValue(Parameters::Rate.key, QVariant::fromValue(m_Rate));
  RateChanged(val);
}

int Model::getBufferSize() const { return m_BufferSize; }

void Model::setBufferSize(int val)
{
  if (val <= 0)
    val = 512;
  if (val == m_BufferSize)
    return;

  m_BufferSize = val;

  QSettings s;
  s.setValue(Parameters::BufferSize.key, QVariant::fromValue(m_BufferSize));
  BufferSizeChanged(val);
}


SCORE_SETTINGS_PARAMETER_CPP(int, Model, DefaultIn)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, DefaultOut)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, AutoStereo)
}
