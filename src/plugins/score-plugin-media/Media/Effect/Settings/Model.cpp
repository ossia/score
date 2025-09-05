#include <Media/Effect/Settings/Model.hpp>

#include <QDir>
#include <QStandardPaths>
namespace Media::Settings
{
namespace Parameters
{

SETTINGS_PARAMETER_IMPL(VstPaths){
    QStringLiteral("Effect/VstPaths"),
    {(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + QDir::separator()
      + ".vst"),
#if defined(__APPLE__)
     "/Library/Audio/Plug-Ins/VST"
#elif defined(__linux__)
     QStringLiteral("/usr/lib/vst"), QStringLiteral("/usr/lib/lxvst"),
     QStringLiteral("/usr/lib64/vst"), QStringLiteral("/usr/lib64/lxvst")
#elif defined(_WIN32)
     QStringLiteral("C:\\Program Files\\VSTPlugins"),
     QStringLiteral("C:\\Program Files\\Steinberg\\VSTPlugins"),
     QStringLiteral("C:\\Program Files\\Common Files\\VST2"),
     QStringLiteral("C:\\Program Files\\Common Files\\Steinberg\\VST2")
#else

     "/usr/lib/vst"

#endif
    }};

SETTINGS_PARAMETER_IMPL(VstAlwaysOnTop){
    QStringLiteral("score_plugin_engine/VstAlwaysOnTop"), true};
static auto list()
{
  return std::tie(VstPaths, VstAlwaysOnTop);
}
}

auto VstPathsChanged_symbol_for_shlib_bug = &Media::Settings::Model::VstPathsChanged;
Model::Model(QSettings& set, const score::ApplicationContext& ctx)
{
  score::setupDefaultSettings(set, Parameters::list(), *this);
}

SCORE_SETTINGS_PARAMETER_CPP(QStringList, Model, VstPaths)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, VstAlwaysOnTop)
}
