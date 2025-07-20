#pragma once
#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <QObject>

#include <memory>
#include <thread>
#include <vector>
#include <verdigris>

namespace Library
{
class ProcessesItemModel;
}

namespace Clap
{
class ApplicationPlugin
    : public QObject
    , public score::GUIApplicationPlugin
{
  W_OBJECT(ApplicationPlugin)

public:
  explicit ApplicationPlugin(const score::GUIApplicationContext& app);
  ~ApplicationPlugin();

  void initialize() override;

  struct PluginInfo
  {
    QString path;
    QString id;
    QString name;
    QString vendor;
    QString version;
    std::vector<QString> features;
  };

  const std::vector<PluginInfo>& plugins() const noexcept { return m_plugins; }

public:
  void pluginsChanged() W_SIGNAL(pluginsChanged);

private:
  void rescanPlugins();
  void scanClapPlugins();

  std::vector<PluginInfo> m_plugins;
  std::thread m_scanThread;
};
}
