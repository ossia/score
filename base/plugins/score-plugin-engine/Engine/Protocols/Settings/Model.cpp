#include <Engine/Protocols/Settings/Model.hpp>
namespace Audio::Settings
{
namespace Parameters
{
const score::sp<ModelDriverParameter> Driver{QStringLiteral("Audio/Driver"), "PortAudio"};
const score::sp<ModelCardInParameter> CardIn{QStringLiteral("Audio/CardIn"), ""};
const score::sp<ModelCardOutParameter> CardOut{QStringLiteral("Audio/CardOut"), ""};
const score::sp<ModelBufferSizeParameter> BufferSize{QStringLiteral("Audio/BufferSize"), 64};
const score::sp<ModelRateParameter> Rate{QStringLiteral("Audio/SamplingRate"), 44100};
const score::sp<ModelDefaultInParameter> DefaultIn{QStringLiteral("Audio/DefaultIn"), 8};
const score::sp<ModelDefaultOutParameter> DefaultOut{QStringLiteral("Audio/DefaultOut"), 8};

static auto list()
{
  return std::tie(Driver, Rate, CardIn, CardOut, BufferSize, DefaultIn, DefaultOut);
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

