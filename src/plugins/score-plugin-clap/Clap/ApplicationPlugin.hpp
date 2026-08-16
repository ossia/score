#pragma once
#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <QObject>
#include <QtCore/qglobal.h>

#include <score_plugin_clap_export.h>

#include <memory>
#include <vector>
#include <verdigris>

namespace Library
{
class ProcessesItemModel;
}
namespace Media
{
class PluginScanner;
}

class QTimer;
class QJsonObject;

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

//! The plug-ins contained in one .clap file, from a clappuppet scan reply.
//! The path is the scanned file, not whatever the reply claims.
SCORE_PLUGIN_CLAP_EXPORT
std::vector<PluginInfo> parseClapReply(const QString& path, const QJsonObject& obj);

//! On macOS a .clap can be a bundle directory; the actual binary to scan
//! lives inside. Must be applied *before* comparing against known paths:
//! resolving after the known-check made every startup rescan (and
//! re-append) every bundle.
SCORE_PLUGIN_CLAP_EXPORT
QString resolveClapEntry(const QString& path);

class SCORE_PLUGIN_CLAP_EXPORT ApplicationPlugin
    : public QObject
    , public score::GUIApplicationPlugin
{
  W_OBJECT(ApplicationPlugin)

public:
  explicit ApplicationPlugin(const score::GUIApplicationContext& app);
  ~ApplicationPlugin();

  void initialize() override;

  const std::vector<PluginInfo>& plugins() const noexcept { return m_plugins; }

  void rescanPlugins();
  //! Forget everything and scan the configured paths again (settings UI)
  void forceRescan();

public:
  void pluginsChanged() E_SIGNAL(SCORE_PLUGIN_CLAP_EXPORT, pluginsChanged);

private:
  void onScanned(const QString& path, const QJsonObject& obj);
  void onScanFailed(const QString& path, const QString& reason);
  void removeEntriesForPath(const QString& path);
  void persistCache();
  void schedulePersist();

#if QT_CONFIG(process)
  std::unique_ptr<Media::PluginScanner> m_scanner;
#endif
  QTimer* m_persistTimer{};

  std::vector<PluginInfo> m_plugins;
};
}

Q_DECLARE_METATYPE(Clap::PluginInfo)
Q_DECLARE_METATYPE(std::vector<Clap::PluginInfo>)
