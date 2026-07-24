#include <Audio/DummyInterface.hpp>
#include <Audio/Settings/Model.hpp>

#include <score/application/GUIApplicationContext.hpp>

#include <optional>
namespace Audio::Settings
{
namespace Parameters
{
// portaudio is the default
SETTINGS_PARAMETER_IMPL(Driver)
{
  QStringLiteral("Audio/Driver"),
#if defined(_WIN32)
      Audio::AudioFactory::ConcreteKey{score::uuids::string_generator::compute(
          "2838b52b-9c0c-466e-a2b0-8c6d8cb8fc6d")} // WASAPI (miniaudio)
#elif defined(__APPLE__)
    Audio::AudioFactory::ConcreteKey{
        score::uuids::string_generator::compute("85115103-694a-4a3b-9274-76ef47aec5a9")}
#elif defined(__linux__)
      Audio::AudioFactory::ConcreteKey{score::uuids::string_generator::compute(
          "687d49cf-b58d-430f-8358-ec02cb50be36")} // PipeWire
#elif defined(__EMSCRIPTEN__)
      Audio::AudioFactory::ConcreteKey{score::uuids::string_generator::compute(
          "a2e8b688-521c-4755-b0b2-4e3d01807b8e")} // Web Audio (MiniAudio)
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
SETTINGS_PARAMETER_IMPL(JackTransport){
    QStringLiteral("Audio/JackTransport"), ExternalTransport::None};

static auto list()
{
  return std::tie(
      Rate, InputNames, OutputNames, CardIn, CardOut, BufferSize, DefaultIn, DefaultOut,
      AutoStereo, AutoConnect, JackTransport, Driver);
}
}

// Maps the SCORE_AUDIO_BACKEND environment variable to a concrete driver key.
// Used both at startup (so headless/CI/test runs can force a backend such as
// "dummy") and from the settings GUI. Returns std::nullopt when the variable is
// unset or unrecognized.
static std::optional<Audio::AudioFactory::ConcreteKey> driverFromEnv()
{
  auto env = qEnvironmentVariable("SCORE_AUDIO_BACKEND").toLower();
  if(env.isEmpty())
    return std::nullopt;

  QByteArray uid;
  if(env == "dummy")
    uid = "13dabcc3-9cda-422f-a8c7-5fef5c220677";
  else if(env == "jack")
    uid = "7ff2af00-f2f5-4930-beec-0e2d21eda195";
  else if(env == "sdl")
    uid = "28b88e91-c5f0-4f13-834f-aa333d14aa81";
  else if(env == "pipewire")
    uid = "687d49cf-b58d-430f-8358-ec02cb50be36";
  else if(env == "coreaudio")
    uid = "85115103-694a-4a3b-9274-76ef47aec5a9";
  else if(env == "alsa")
    uid = "a390218a-a951-4cda-b4ee-c41d2df44236";
  else if(env == "alsa_portaudio")
    uid = "3533ee88-9a8d-486c-b20b-6c966cf4eaa0";
  else if(env == "alsa_miniaudio")
    uid = "e0c533da-a1f4-4795-90b5-a805cdfcb79f";
  else if(env == "wasapi_miniaudio")
    uid = "2838b52b-9c0c-466e-a2b0-8c6d8cb8fc6d";

  if(uid.isEmpty())
    return std::nullopt;

  const char* data = uid.data();
  using uid_t = const char(&)[37];
  return Audio::AudioFactory::ConcreteKey{
      score::uuids::string_generator::compute((uid_t)*data)};
}

Model::Model(
    const UuidKey<score::SettingsDelegateFactory>& k, QSettings& set,
    const score::ApplicationContext& ctx)
    : score::SettingsDelegateModel{k, nullptr}
{
  score::setupDefaultSettings(set, Parameters::list(), *this);
  using namespace std::literals;
  if(m_Driver
     == Audio::AudioFactory::ConcreteKey::fromString(
         "85115103-694a-4a3b-9274-76ef47aec5a9"s))
  {
    m_Driver = Parameters::Driver.def;
  }

  // Allow forcing the audio backend at startup, e.g. SCORE_AUDIO_BACKEND=dummy
  // for headless / CI / test runs.
  if(auto k = driverFromEnv())
    m_Driver = *k;
}

Audio::AudioFactory::ConcreteKey Model::getDriver() const
{
  return m_Driver;
}

void Model::initDriver(Audio::AudioFactory::ConcreteKey val)
{
  if(auto k = driverFromEnv())
    val = *k;

  // Reset to default in case of invalid parameters.
  auto& ctx = score::AppContext();
  auto& factories = ctx.interfaces<AudioFactoryList>();
  auto iface = factories.get(val);
  if(!iface)
  {
    val = Parameters::Driver.def;
    iface = factories.get(val);
    if(!iface)
    {
      val = DummyFactory::static_concreteKey();
      iface = factories.get(val);
    }
  }

  if(val == m_Driver)
    return;

  m_Driver = val;

  // SDL
  if(m_Driver
     == Audio::AudioFactory::ConcreteKey{score::uuids::string_generator::compute(
         "28b88e91-c5f0-4f13-834f-aa333d14aa81")})
  {
    if(m_Rate != 48000)
    {
      m_Rate = 48000;
      RateChanged(m_Rate);
    }
    if(m_BufferSize != 1024)
    {
      m_BufferSize = 1024;
      BufferSizeChanged(m_BufferSize);
    }
  }

#if !defined(__EMSCRIPTEN__)
  // Special case for dummy driver: set reasonable values
  if(m_Driver
     == Audio::AudioFactory::ConcreteKey{score::uuids::string_generator::compute(
         "13dabcc3-9cda-422f-a8c7-5fef5c220677")})
  {
    if(m_Rate < 1 || m_Rate > 48 * 384000)
    {
      m_Rate = 44100;
      RateChanged(m_Rate);
    }
    if(m_BufferSize < 1 || m_BufferSize > 32768 * 8)
    {
      m_BufferSize = 128;
      BufferSizeChanged(m_BufferSize);
    }
  }
#endif
  iface->initialize(*this, ctx);
}
void Model::setDriver(Audio::AudioFactory::ConcreteKey val)
{
  // Reset to default in case of invalid parameters.
  auto& ctx = score::AppContext();
  auto& factories = ctx.interfaces<AudioFactoryList>();
  auto iface = factories.get(val);
  if(!iface)
  {
    val = Parameters::Driver.def;
    iface = factories.get(val);
    if(!iface)
    {
      val = DummyFactory::static_concreteKey();
      iface = factories.get(val);
    }
  }

  if(val == m_Driver)
    return;

  m_Driver = val;

  // SDL
  if(m_Driver
     == Audio::AudioFactory::ConcreteKey{score::uuids::string_generator::compute(
         "28b88e91-c5f0-4f13-834f-aa333d14aa81")})
  {
    setRate(48000);
    setBufferSize(1024);
  }

#if !defined(__EMSCRIPTEN__)
  QSettings s;
  s.setValue(Parameters::Driver.key, QVariant::fromValue(m_Driver));

  // Special case for dummy driver: set reasonable values
  if(m_Driver
     == Audio::AudioFactory::ConcreteKey{score::uuids::string_generator::compute(
         "13dabcc3-9cda-422f-a8c7-5fef5c220677")})
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

void Model::initRate(int val)
{
  if(val <= 0)
    val = 44100;
  if(val == m_Rate)
    return;

  m_Rate = val;
}

void Model::setRate(int val)
{
  if(val <= 0)
    val = 44100;
  if(val == m_Rate)
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

void Model::initBufferSize(int val)
{
  if(val <= 0)
    val = 512;
  if(val == m_BufferSize)
    return;

  m_BufferSize = val;
}

void Model::setBufferSize(int val)
{
  if(val <= 0)
    val = 512;
  if(val == m_BufferSize)
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
