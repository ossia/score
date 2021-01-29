#pragma once
#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <verdigris>
#include <Vst/Loader.hpp>

#include <QElapsedTimer>
#include <QWebSocketServer>

#include <ossia/detail/hash_map.hpp>

#include <QProcess>

#include <score_plugin_media_export.h>

#include <thread>
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

class SCORE_PLUGIN_MEDIA_EXPORT ApplicationPlugin
    : public QObject
    , public score::ApplicationPlugin
{
  W_OBJECT(ApplicationPlugin)
public:
  ApplicationPlugin(const score::ApplicationContext& app);
  void initialize() override;
  ~ApplicationPlugin() override;

  void rescanVSTs(const QStringList&);
  void processIncomingMessage(const QString& txt);
  void addInvalidVST(const QString& path);
  void addVST(const QString& path, const QJsonObject& json);

  void scanVSTsEvent();

  void vstChanged() W_SIGNAL(vstChanged)

  std::vector<VSTInfo> vst_infos;
  ossia::fast_hash_map<int32_t, vst::Module*> vst_modules;

  const std::thread::id m_tid{std::this_thread::get_id()};
  auto mainThreadId() const noexcept { return m_tid; }

  struct ScanningProcess
  {
    QString path;
    std::unique_ptr<QProcess> process;
    bool scanning{};
    QElapsedTimer timer;
  };

  std::vector<ScanningProcess> m_processes;

  QWebSocketServer m_wsServer;

};

class GUIApplicationPlugin : public QObject, public score::GUIApplicationPlugin
{
public:
  GUIApplicationPlugin(const score::GUIApplicationContext& app);
  void initialize() override;
};
}
