#include <Media/Effect/Settings/Model.hpp>

#include <QDir>
namespace Media::Settings
{
namespace Parameters
{

SETTINGS_PARAMETER_IMPL(VstPaths){
    QStringLiteral("Effect/VstPaths"),
    {(QString::fromUtf8(qgetenv("HOME")) + QDir::separator() + ".vst"),
#if defined(__APPLE__)
     "/Library/Audio/Plug-Ins/VST"
#elif defined(__linux__)
     QStringLiteral("/usr/lib/vst"), QStringLiteral("/usr/lib/lxvst"),
     QStringLiteral("/usr/lib64/vst"), QStringLiteral("/usr/lib64/lxvst")
#elif defined(_WIN32)
     "C:\\Program Files\\VSTPlugins", "C:\\Program Files\\Steinberg\\VSTPlugins",
     "C:\\Program Files\\Common Files\\VST2",
     "C:\\Program Files\\Common Files\\Steinberg\\VST2"
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

Model::Model(QSettings& set, const score::ApplicationContext& ctx)
{
  score::setupDefaultSettings(set, Parameters::list(), *this);
}

SCORE_SETTINGS_PARAMETER_CPP(QStringList, Model, VstPaths)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, VstAlwaysOnTop)
}
