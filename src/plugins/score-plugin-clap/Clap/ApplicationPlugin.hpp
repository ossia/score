#pragma once
#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <QObject>
#include <QWebSocketServer>

#include <memory>
#include <vector>
#include <map>
#include <verdigris>

namespace Library
{
class ProcessesItemModel;
}

class QProcess;

namespace Clap
{
struct PluginInfo
{
  QString path;
  QString id;
  QString name;
  QString vendor;
  QString version;
  QString url;
  QString manual_url;
  QString support_url;
  QString description;

  QList<QString> features;
  bool valid{};
};

class ApplicationPlugin
    : public QObject
    , public score::GUIApplicationPlugin
{
  W_OBJECT(ApplicationPlugin)

public:
  explicit ApplicationPlugin(const score::GUIApplicationContext& app);
  ~ApplicationPlugin();

  void initialize() override;

  const std::vector<PluginInfo>& plugins() const noexcept { return m_plugins; }

public:
  void pluginsChanged() W_SIGNAL(pluginsChanged);

private:
  void rescanPlugins();
  void scanNextBatch();
  void processIncomingMessage(const QString& txt);
  void addPlugin(const QString& path, const QJsonObject& obj);
  void addInvalidPlugin(const QString& path);
  
  QWebSocketServer m_wsServer;
  std::map<int, QProcess*> m_processes;
  std::vector<QString> m_pluginQueue;
  int m_processCount{0};
  static constexpr int max_in_flight = 8;

  std::vector<PluginInfo> m_plugins;
};
}

Q_DECLARE_METATYPE(Clap::PluginInfo)
Q_DECLARE_METATYPE(std::vector<Clap::PluginInfo>)
