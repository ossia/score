#include <Audio/Settings/Model.hpp>
#include <Audio/DummyInterface.hpp>
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
    score::uuids::string_generator::compute("afcd9c64-0367-4fa1-b2bb-ee65b1c5e5a7")
  } // WASAPI
#elif defined(__APPLE__)
      Audio::AudioFactory::ConcreteKey
  {
    score::uuids::string_generator::compute("e75cb711-613f-4f15-834f-398ab1807470")
  }
#elif defined(__linux__)
      Audio::AudioFactory::ConcreteKey
  {
    score::uuids::string_generator::compute("3533ee88-9a8d-486c-b20b-6c966cf4eaa0")
  } // ALSA
#elif defined(__EMSCRIPTEN__)
      Audio::AudioFactory::ConcreteKey
  {
    score::uuids::string_generator::compute("28b88e91-c5f0-4f13-834f-aa333d14aa81")
  } // ALSA
#else
      Audio::AudioFactory::ConcreteKey
  {
    score::uuids::string_generator::compute("e7543875-3b22-457c-bf41-75504637686f")
  }
#endif
};
SETTINGS_PARAMETER_IMPL(InputNames){QStringLiteral("Audio/InputNames"), {}};
SETTINGS_PARAMETER_IMPL(OutputNames){QStringLiteral("Audio/OutputNames"), {}};
SETTINGS_PARAMETER_IMPL(CardIn){QStringLiteral("Audio/CardIn"), ""};
SETTINGS_PARAMETER_IMPL(CardOut){QStringLiteral("Audio/CardOut"), ""};
SETTINGS_PARAMETER_IMPL(BufferSize){QStringLiteral("Audio/BufferSize"), 512};
SETTINGS_PARAMETER_IMPL(Rate){QStringLiteral("Audio/SamplingRate"), 44100};
SETTINGS_PARAMETER_IMPL(DefaultIn){QStringLiteral("Audio/DefaultIn"), 2};
SETTINGS_PARAMETER_IMPL(DefaultOut){QStringLiteral("Audio/DefaultOut"), 2};
SETTINGS_PARAMETER_IMPL(AutoStereo){QStringLiteral("Audio/AutoStereo"), true};
SETTINGS_PARAMETER_IMPL(AutoConnect){QStringLiteral("Audio/AutoConnect"), true};
SETTINGS_PARAMETER_IMPL(JackTransport){QStringLiteral("Audio/JackTransport"), ExternalTransport::None};

static auto list()
{
  return std::tie(Driver, Rate, InputNames, OutputNames, CardIn, CardOut, BufferSize, DefaultIn, DefaultOut, AutoStereo, AutoConnect, JackTransport);
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
  auto& ctx = score::AppContext();
  auto& factories = ctx.interfaces<AudioFactoryList>();
  auto iface = factories.get(val);
  if (!iface)
  {
    val = Parameters::Driver.def;
    iface = factories.get(val);
    if (!iface)
    {
      val = DummyFactory::static_concreteKey();
      iface = factories.get(val);
    }
  }

  if (val == m_Driver)
    return;

  m_Driver = val;

  // SDL
  if(m_Driver == Audio::AudioFactory::ConcreteKey{
          score::uuids::string_generator::compute("28b88e91-c5f0-4f13-834f-aa333d14aa81")})
  {
    setRate(48000);
    setBufferSize(1024);
  }

#if !defined(__EMSCRIPTEN__)
  QSettings s;
  s.setValue(Parameters::Driver.key, QVariant::fromValue(m_Driver));

  // Special case for dummy driver: set reasonable values
  if (m_Driver
      == Audio::AudioFactory::ConcreteKey{
          score::uuids::string_generator::compute("13dabcc3-9cda-422f-a8c7-5fef5c220677")})
  {
    m_Rate = 44100;
    s.setValue(Parameters::Rate.key, QVariant::fromValue(m_Rate));

    m_BufferSize = 1024;
    s.setValue(Parameters::BufferSize.key, QVariant::fromValue(m_BufferSize));
  }
#endif
  iface->initialize(*this, ctx);
}

int Model::getRate() const
{
  return m_Rate;
}

void Model::setRate(int val)
{
  if (val <= 0)
    val = 44100;
  if (val == m_Rate)
    return;

  m_Rate = val;

#if !defined(__EMSCRIPTEN__)
  QSettings s;
  s.setValue(Parameters::Rate.key, QVariant::fromValue(m_Rate));
#endif
  RateChanged(val);
}

int Model::getBufferSize() const
{
  return m_BufferSize;
}

void Model::setBufferSize(int val)
{
  if (val <= 0)
    val = 512;
  if (val == m_BufferSize)
    return;

  m_BufferSize = val;

#if !defined(__EMSCRIPTEN__)
  QSettings s;
  s.setValue(Parameters::BufferSize.key, QVariant::fromValue(m_BufferSize));
#endif
  BufferSizeChanged(val);
}

SCORE_SETTINGS_PARAMETER_CPP(QStringList, Model, InputNames)
SCORE_SETTINGS_PARAMETER_CPP(QStringList, Model, OutputNames)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, CardIn)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, CardOut)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, DefaultIn)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, DefaultOut)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, AutoStereo)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, AutoConnect)
SCORE_SETTINGS_PARAMETER_CPP(Audio::Settings::ExternalTransport, Model, JackTransport)
}
