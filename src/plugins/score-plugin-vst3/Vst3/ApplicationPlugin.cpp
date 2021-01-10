#include <Vst3/ApplicationPlugin.hpp>
#include <QDir>
#include <QDirIterator>
W_OBJECT_IMPL(vst3::ApplicationPlugin)

namespace vst3
{
namespace
{
#if defined(__APPLE__)
static const constexpr auto default_path = "/Library/Audio/Plug-Ins/VST";
static const constexpr auto default_filter = "*.vst3";
#elif defined(__linux__)
static const constexpr auto default_path{"/usr/lib/vst3"};
static const constexpr auto default_filter = "*.vst3";
#elif defined(_WIN32)
static const constexpr auto default_path = "c:\\vst";
static const constexpr auto default_filter = "*.vst3";
#else
static const constexpr auto default_path = "";
static const constexpr auto default_filter = "";
#endif
}
ApplicationPlugin::ApplicationPlugin(const score::ApplicationContext& ctx):
  score::ApplicationPlugin{ctx}
{
}
void ApplicationPlugin::initialize()
{
  rescan({default_path});
}

void ApplicationPlugin::rescan(const QStringList& paths)
{
  QSet<QString> newPlugins;
  for (const QString& dir : paths)
  {
    QDirIterator it(
          dir,
          QStringList{default_filter},
          QDir::Dirs,
          QDirIterator::Subdirectories | QDirIterator::FollowSymlinks);
    while (it.hasNext())
      newPlugins.insert(it.next());
  }

  for(const auto& vst : newPlugins)
  {
    try {
      auto module = getModule(vst.toStdString());
      auto& v = vst_infos.emplace_back(AvailablePlugin{vst});
      v.module = module;

      const auto& factory = module->getFactory();
      for (const auto& class_info : factory.classInfos())
      {
        if (class_info.category() == kVstAudioEffectClass)
        {
          v.classInfo.push_back(class_info);
        }
      }

      v.isValid = v.classInfo.size() > 0;
    } catch(std::exception& e) {
      qDebug() << e.what();
    }
  }

  vstChanged();
}

}
