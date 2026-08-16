#pragma once
#include <Vst3/Plugin.hpp>

#include <score/plugins/application/GUIApplicationPlugin.hpp>

#include <ossia/detail/fmt.hpp>
#include <ossia/detail/string_map.hpp>

#include <QtCore/qglobal.h>

#include <score_plugin_vst3_export.h>

#include <base/source/fstring.h>
#include <pluginterfaces/vst/ivstmessage.h>

#include <memory>
#include <stdexcept>
#include <verdigris>

class QTimer;
class QJsonObject;
namespace Media
{
class PluginScanner;
}

namespace vst3
{
struct AvailablePlugin
{
  QString path;
  QString name;
  QString url;
  std::vector<VST3::Hosting::ClassInfo> classInfo;

  bool isValid{};
};

//! Build an AvailablePlugin from a vst3puppet scan reply. The path is the
//! scanned module, not whatever the reply claims.
SCORE_PLUGIN_VST3_EXPORT
AvailablePlugin parseVst3Reply(const QString& path, const QJsonObject& obj);

struct HostApp final : public Steinberg::Vst::IHostApplication
{
  Steinberg::Vst::PlugInterfaceSupport m_support;
  HostApp() { }
  virtual ~HostApp() { }
  Steinberg::tresult PLUGIN_API getName(Steinberg::Vst::String128 name) override
  {
    Steinberg::String str("ossia score");
    str.copyTo16(name, 0, 127);
    return Steinberg::kResultTrue;
  }

  Steinberg::tresult
  PLUGIN_API createInstance(Steinberg::TUID cid, Steinberg::TUID _iid, void** obj) override
  {
    using namespace Steinberg;
    using namespace Steinberg::Vst;
    FUID classID(FUID::fromTUID(cid));
    FUID interfaceID(FUID::fromTUID(_iid));
    if(classID == Vst::IMessage::iid && interfaceID == Vst::IMessage::iid)
    {
      *obj = new HostMessage;
      return kResultTrue;
    }
    else if(
        classID == Vst::IAttributeList::iid && interfaceID == Vst::IAttributeList::iid)
    {
      *obj = Vst::HostAttributeList::make();
      return kResultTrue;
    }
    *obj = nullptr;
    return kResultFalse;
  }

  Steinberg::tresult PLUGIN_API queryInterface(const char* _iid, void** obj) override
  {
    using namespace Steinberg;
    using namespace Steinberg::Vst;
    QUERY_INTERFACE(_iid, obj, FUnknown::iid, IHostApplication)
    QUERY_INTERFACE(_iid, obj, IHostApplication::iid, IHostApplication)

    if(m_support.isPlugInterfaceSupported(_iid) == kResultTrue)
      return kResultOk;

    *obj = nullptr;
    return kResultFalse;
  }

  Steinberg::uint32 PLUGIN_API addRef() override { return 1; }

  Steinberg::uint32 PLUGIN_API release() override { return 1; }
};

class SCORE_PLUGIN_VST3_EXPORT ApplicationPlugin
    : public QObject
    , public score::ApplicationPlugin
{
  W_OBJECT(ApplicationPlugin)
public:
  ApplicationPlugin(const score::ApplicationContext& ctx);
  ~ApplicationPlugin();

  void initialize() override;

  VST3::Hosting::Module::Ptr getModule(const std::string& path);

  void rescan();
  void rescan(const QStringList& paths);
  void vstChanged() E_SIGNAL(SCORE_PLUGIN_VST3_EXPORT, vstChanged);

  std::pair<const AvailablePlugin*, const VST3::Hosting::ClassInfo*>
  classInfo(const VST3::UID& uid) const noexcept;
  QString pathForClass(const VST3::UID& uid) const noexcept;
  std::optional<VST3::UID>
  uidForPathAndClassName(const QString& path, const QString& cls) const noexcept;

  HostApp m_host;
  ossia::string_map<VST3::Hosting::Module::Ptr> modules;
  std::vector<AvailablePlugin> vst_infos;

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
  bool m_scanRan{};
};
}
