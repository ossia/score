#pragma once
#include <Vst/Loader.hpp>

#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <ossia/detail/hash_map.hpp>

#include <QtCore/qglobal.h>

#include <score_plugin_vst_export.h>

#include <thread>
#include <verdigris>

class QTimer;
class QJsonObject;
namespace Media
{
class PluginScanner;
}

namespace vst
{
struct VSTInfo
{
  QString path;
  QString prettyName;
  QString displayName;
  QString author;
  int32_t uniqueID{};
  int32_t controls{};
  bool isSynth{};
  bool isValid{};
};

//! Build a VSTInfo from a vstpuppet scan reply. The path is the scanned
//! file, not whatever the reply claims.
SCORE_PLUGIN_VST_EXPORT
VSTInfo parseVstReply(const QString& path, const QJsonObject& obj);

class Model;
class ApplicationPlugin
    : public QObject
    , public score::ApplicationPlugin
{
  W_OBJECT(ApplicationPlugin)
public:
  ApplicationPlugin(const score::ApplicationContext& app);
  void initialize() override;
  ~ApplicationPlugin() override;

  void clearVSTs();
  void rescanVSTs(QStringList);

  // Used for idle timers
  void registerRunningVST(vst::Model*);
  void unregisterRunningVST(vst::Model*);

  void vstChanged() E_SIGNAL(SCORE_PLUGIN_VST_EXPORT, vstChanged);

  std::vector<VSTInfo> vst_infos;
  ossia::hash_map<int32_t, vst::Module*> vst_modules;

  const std::thread::id m_tid{std::this_thread::get_id()};
  auto mainThreadId() const noexcept { return m_tid; }
  std::vector<vst::Model*> m_runningVSTs;

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

  void timerEvent(QTimerEvent* event) override;
};

class GUIApplicationPlugin
    : public QObject
    , public score::GUIApplicationPlugin
{
public:
  GUIApplicationPlugin(const score::GUIApplicationContext& app);
  void initialize() override;
};
}
