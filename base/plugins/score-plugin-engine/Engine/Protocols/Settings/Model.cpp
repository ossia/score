#include <Engine/Protocols/Settings/Model.hpp>
namespace Audio::Settings
{
namespace Parameters
{
sp_(Driver){QStringLiteral("Audio/Driver"), "PortAudio"};
sp_(CardIn){QStringLiteral("Audio/CardIn"), ""};
sp_(CardOut){QStringLiteral("Audio/CardOut"), ""};
sp_(BufferSize){QStringLiteral("Audio/BufferSize"), 64};
sp_(Rate){QStringLiteral("Audio/SamplingRate"), 44100};
sp_(DefaultIn){QStringLiteral("Audio/DefaultIn"), 8};
sp_(DefaultOut){QStringLiteral("Audio/DefaultOut"), 8};

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

SCORE_SETTINGS_PARAMETER_CPP(QString, Model, Driver)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, CardIn)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, CardOut)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, BufferSize)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, Rate)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, DefaultIn)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, DefaultOut)
}
