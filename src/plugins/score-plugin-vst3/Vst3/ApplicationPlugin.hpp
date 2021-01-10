#pragma once
#include <Vst3/Plugin.hpp>

#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <stdexcept>
#include <ossia/detail/fmt.hpp>

namespace vst3
{

struct vst_error: public std::runtime_error
{
  template<typename... Args>
  vst_error(Args&&... args) noexcept
    : runtime_error{fmt::format(args...)}
  {

  }
};

struct ApplicationPlugin
    : public QObject
    , public score::ApplicationPlugin
{
  W_OBJECT(ApplicationPlugin)
public:
  ApplicationPlugin(const score::ApplicationContext& ctx);

  void initialize() override;

  void rescan(const QStringList& paths);
  void vstChanged() W_SIGNAL(vstChanged)

  struct AvailablePlugin
  {
    QString path;
    VST3::Hosting::Module::Ptr module;
    std::vector<VST3::Hosting::ClassInfo> classInfo;

    bool isValid{};
  };

  VST3::Hosting::Module::Ptr getModule(const std::string& path)
  {
    std::string err;
    auto it = modules.find(path);
    if(it != modules.end())
    {
      return it->second;
    }
    else
    {
      auto module = VST3::Hosting::Module::create(path, err);

      if (!module)
        throw vst_error("Failed to load VST3 ({}) : {}", path, err);

      modules[path] = module;
      return module;
    }
  }

  Steinberg::Vst::HostApplication m_host;
  ossia::string_map<VST3::Hosting::Module::Ptr> modules;
  std::vector<AvailablePlugin> vst_infos;



};
}
