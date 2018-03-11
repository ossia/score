#include <Media/Effect/Settings/Model.hpp>
namespace Media::Settings
{
namespace Parameters
{
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
  return std::tie(VstPaths);
}
}

Model::Model(QSettings& set, const score::ApplicationContext& ctx)
{
  score::setupDefaultSettings(set, Parameters::list(), *this);
}

SCORE_SETTINGS_PARAMETER_CPP(QStringList, Model, VstPaths)
}

