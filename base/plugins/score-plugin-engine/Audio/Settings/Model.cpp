#include <Audio/Settings/Model.hpp>
namespace Audio::Settings
{
namespace Parameters
{
SETTINGS_PARAMETER_IMPL(Driver){QStringLiteral("Audio/Driver"), "PortAudio"};
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

SCORE_SETTINGS_PARAMETER_CPP(QString, Model, Driver)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, CardIn)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, CardOut)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, BufferSize)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, Rate)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, DefaultIn)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, DefaultOut)
}
