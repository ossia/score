#pragma once
#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <verdigris>
#if defined(HAS_LV2)
#include <Media/Effect/LV2/LV2Context.hpp>

#include <lilv/lilvmm.hpp>
#endif

#if defined(HAS_VST2)
#include <Media/Effect/VST/VSTLoader.hpp>

#include <QElapsedTimer>
#include <QWebSocketServer>
#endif

#include <ossia/detail/hash_map.hpp>

#include <QProcess>

#include <score_plugin_media_export.h>

#include <thread>
namespace Media
{
namespace LV2
{
struct HostContext;
struct GlobalContext;
}
class SCORE_PLUGIN_MEDIA_EXPORT ApplicationPlugin : public QObject, public score::ApplicationPlugin
{
  W_OBJECT(ApplicationPlugin)
public:
  ApplicationPlugin(const score::ApplicationContext& app);
  void initialize() override;
  ~ApplicationPlugin() override;

#if defined(HAS_LV2) // TODO instead add a proper preprocessor macro that
                     // also works in static case
public:
  Lilv::World lilv;
  std::unique_ptr<LV2::GlobalContext> lv2_context;
  LV2::HostContext lv2_host_context;
#endif

#if defined(HAS_VST2)
public:
  void rescanVSTs(const QStringList&);
  void vstChanged() W_SIGNAL(vstChanged)

  struct vst_info
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
  std::vector<vst_info> vst_infos;
  ossia::fast_hash_map<int32_t, Media::VST::VSTModule*> vst_modules;

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

  void addInvalidVST(const QString& path);
  void addVST(const QString& path, const QJsonObject& json);

  void scanVSTsEvent();
#endif
};

class GUIApplicationPlugin : public QObject, public score::GUIApplicationPlugin
{
public:
  GUIApplicationPlugin(const score::GUIApplicationContext& app);
  void initialize() override;
};
}
