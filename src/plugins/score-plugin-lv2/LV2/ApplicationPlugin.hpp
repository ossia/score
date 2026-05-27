#pragma once
#include <LV2/Context.hpp>
#include <LV2/Suil.hpp>

#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <ossia/detail/hash_map.hpp>

#include <QPointer>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QWebSocketServer>

#include <lilv/lilvmm.hpp>

#include <map>
#include <vector>
#include <verdigris>

namespace LV2
{
struct HostContext;
struct GlobalContext;

// Cached descriptor from a lv2puppet scan; persisted to QSettings("Effect/KnownLV2")
struct PluginInfo
{
  QString bundle;            // file system path to .lv2 bundle dir
  QString uri;               // canonical plug-in URI
  QString name;
  QString class_label;       // lv2:Class label (Reverb, Delay, ...)
  QString author;
  QString documentationLink; // author homepage URL

  // Cache invalidation key — re-scan when the manifest changes.
  qint64 mtime{};
  qint64 size{};

  int audio_in{};
  int audio_out{};
  int midi_in{};
  int midi_out{};
  int control_in{};
  int control_out{};
  int atom_in{};
  int atom_out{};
  int cv_in{};

  bool has_ui{};
  QStringList ui_uris;
  QStringList preset_uris;
  // Parallel to preset_uris: bundle that must be loaded before lilv_plugin_get_related
  QStringList preset_bundles;

  bool valid{};
};

class ApplicationPlugin
    : public QObject
    , public score::ApplicationPlugin
{
  W_OBJECT(ApplicationPlugin)
public:
  explicit ApplicationPlugin(const score::ApplicationContext& app);
  void initialize() override;
  ~ApplicationPlugin() override;

  const std::vector<PluginInfo>& cachedDescriptors() const noexcept
  {
    return m_plugins;
  }

  const PluginInfo* findDescriptor(const QString& uri_or_bundle) const noexcept;

  // Idempotent; main thread only.
  void ensureBundleLoaded(const QString& uri_or_bundle);

public:
  void descriptorsChanged() W_SIGNAL(descriptorsChanged);

public:
  Lilv::World lilv;
  std::unique_ptr<LV2::GlobalContext> lv2_context;
  LV2::HostContext lv2_host_context;

  const libsuil& suil = libsuil::instance();

private:
  void rescanBundles();
  void scanNextBatch();
  void processIncomingMessage(const QString& txt);
  void addPluginsFromJson(const QString& bundlePath, const class QJsonArray& arr);
  void markBundleInvalid(const QString& bundlePath);
  void persistCache();

  static QStringList discoverLv2Search();
  static QStringList discoverBundles(const QStringList& search);
  // Pick out LV2 spec bundles (atom, urid, ui, ...) so puppets can skip load_all
  static QStringList discoverSpecBundles(const QStringList& bundles);

  QWebSocketServer m_wsServer;
  std::map<int, QPointer<QProcess>> m_processes;
  std::vector<QString> m_bundleQueue;
  int m_processCount{0};
  static constexpr int max_in_flight = 8;

  // Suppresses markBundleInvalid on post-report puppet exit (close-handshake race)
  ossia::hash_set<QString> m_scanned_ok;

  std::vector<PluginInfo> m_plugins;

  ossia::hash_set<QString> m_loaded_bundles;

  QStringList m_spec_bundles;

  // Coalesces persistCache + descriptorsChanged into one fire per burst
  class QTimer* m_persistTimer{};
};

}

Q_DECLARE_METATYPE(LV2::PluginInfo)
Q_DECLARE_METATYPE(std::vector<LV2::PluginInfo>)
