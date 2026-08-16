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

SETTINGS_PARAMETER_IMPL(Vst3Paths){
    QStringLiteral("Effect/Vst3Paths"),
    {(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.vst3"),
#if defined(__APPLE__)
     (QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
      + "/Library/Audio/Plug-Ins/VST3"),
     QStringLiteral("/Library/Audio/Plug-Ins/VST3")
#elif defined(_WIN32)
     (qEnvironmentVariable("LOCALAPPDATA") + "/Programs/Common/VST3"),
     QStringLiteral("C:\\Program Files\\Common Files\\VST3")
#else
     QStringLiteral("/usr/lib/vst3"), QStringLiteral("/usr/lib64/vst3")
#endif
    }};

SETTINGS_PARAMETER_IMPL(ClapPaths){
    QStringLiteral("Effect/ClapPaths"),
    {(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.clap"),
#if defined(__APPLE__)
     (QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
      + "/Library/Audio/Plug-Ins/CLAP"),
     QStringLiteral("/Library/Audio/Plug-Ins/CLAP")
#elif defined(_WIN32)
     // Env vars, not QStandardPaths::App*Location: these defaults are
     // evaluated in a static initializer where the application identity may
     // not be set yet, and the CLAP convention is %LOCALAPPDATA%\CLAP anyway
     (qEnvironmentVariable("LOCALAPPDATA") + "/CLAP"),
     QStringLiteral("C:/Program Files/Common Files/CLAP"),
     QStringLiteral("C:/Program Files/Common Files/Audio Plugins/CLAP")
#else
     QStringLiteral("/usr/lib/clap"), QStringLiteral("/usr/local/lib/clap"),
     QStringLiteral("/usr/lib64/clap"), QStringLiteral("/usr/local/lib64/clap")
#endif
    }};

SETTINGS_PARAMETER_IMPL(Lv2Paths){
    QStringLiteral("Effect/Lv2Paths"),
    // ~/.lv2/: Linux convention; Ardour also writes user presets here cross-platform
    {(QStandardPaths::writableLocation(QStandardPaths::HomeLocation) + "/.lv2"),
#if defined(__APPLE__)
     (QStandardPaths::writableLocation(QStandardPaths::HomeLocation)
      + "/Library/Audio/Plug-Ins/LV2"),
     QStringLiteral("/Library/Audio/Plug-Ins/LV2"),
     QStringLiteral("/usr/local/lib/lv2"),    // lilv default, Homebrew Intel
     QStringLiteral("/opt/homebrew/lib/lv2"), // Homebrew Apple Silicon
     QStringLiteral("/usr/lib/lv2")           // lilv default
#elif defined(_WIN32)
     QStringLiteral("C:/Program Files/Common Files/LV2"),
     // Env vars, not QStandardPaths::App*Location: these defaults are
     // evaluated in a static initializer where the application identity may
     // not be set yet - and %APPDATA%\LV2 is the lilv/Carla convention
     (qEnvironmentVariable("APPDATA") + "/LV2"),
     (qEnvironmentVariable("LOCALAPPDATA") + "/LV2")
#else
     QStringLiteral("/usr/lib/lv2"), QStringLiteral("/usr/local/lib/lv2"),
     QStringLiteral("/usr/lib64/lv2"), QStringLiteral("/usr/local/lib64/lv2")
#endif
    }};

SETTINGS_PARAMETER_IMPL(VstAlwaysOnTop){
    QStringLiteral("score_plugin_engine/VstAlwaysOnTop"), true};
static auto list()
{
  return std::tie(VstPaths, Vst3Paths, ClapPaths, Lv2Paths, VstAlwaysOnTop);
}
}

auto VstPathsChanged_symbol_for_shlib_bug = &Media::Settings::Model::VstPathsChanged;
auto Vst3PathsChanged_symbol_for_shlib_bug
    = &Media::Settings::Model::Vst3PathsChanged;
auto ClapPathsChanged_symbol_for_shlib_bug
    = &Media::Settings::Model::ClapPathsChanged;
auto Lv2PathsChanged_symbol_for_shlib_bug = &Media::Settings::Model::Lv2PathsChanged;
Model::Model(
    const UuidKey<score::SettingsDelegateFactory>& k, QSettings& set,
    const score::ApplicationContext& ctx)
    : score::SettingsDelegateModel{k, nullptr}
{
  score::setupDefaultSettings(set, Parameters::list(), *this);
}

SCORE_SETTINGS_PARAMETER_CPP(QStringList, Model, VstPaths)
SCORE_SETTINGS_PARAMETER_CPP(QStringList, Model, Vst3Paths)
SCORE_SETTINGS_PARAMETER_CPP(QStringList, Model, ClapPaths)
SCORE_SETTINGS_PARAMETER_CPP(QStringList, Model, Lv2Paths)
SCORE_SETTINGS_PARAMETER_CPP(bool, Model, VstAlwaysOnTop)
}
