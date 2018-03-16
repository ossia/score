#include <Media/Effect/Settings/Model.hpp>
namespace Media::Settings
{
namespace Parameters
{
const score::sp<ModelCardParameter> Card{QStringLiteral("Audio/Card"), ""};
const score::sp<ModelBufferSizeParameter> BufferSize{QStringLiteral("Audio/BufferSize"), 512};
const score::sp<ModelRateParameter> Rate{QStringLiteral("Audio/SamplingRate"), 44100};

const score::sp<ModelVstPathsParameter> VstPaths{
  QStringLiteral("Effect/VstPaths"),
    #if defined(__APPLE__)
      {"/Library/Audio/Plug-Ins/VST"}
    #elif defined(__linux__)
      {"/usr/lib/vst"}
    #else
      {}
    #endif
};
static auto list()
{
  return std::tie(Rate, Card, BufferSize, VstPaths);
}
}

Model::Model(QSettings& set, const score::ApplicationContext& ctx)
{
  score::setupDefaultSettings(set, Parameters::list(), *this);
}

SCORE_SETTINGS_PARAMETER_CPP(QStringList, Model, VstPaths)
SCORE_SETTINGS_PARAMETER_CPP(QString, Model, Card)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, BufferSize)
SCORE_SETTINGS_PARAMETER_CPP(int, Model, Rate)
}

