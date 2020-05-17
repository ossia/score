#pragma once

#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/tools/Metadata.hpp>

#include <LocalTree/Device/LocalDevice.hpp>
#include <score_plugin_engine_export.h>
namespace score
{
class ModelMetadata;
}

namespace Scenario
{
class ProcessModel;
class IntervalModel;
class EventModel;
class TimeSyncModel;
class StateModel;
}

namespace LocalTree
{
class Interval;
class SCORE_PLUGIN_ENGINE_EXPORT DocumentPlugin final : public score::DocumentPlugin
{
public:
  DocumentPlugin(const score::DocumentContext& doc, Id<score::DocumentPlugin> id, QObject* parent);

  ~DocumentPlugin();

  void init();

  void on_documentClosing() override;
  ossia::net::device_base& device() { return *m_localDevice; }
  const ossia::net::device_base& device() const { return *m_localDevice; }

  Protocols::LocalDevice& localDevice() { return m_localDeviceWrapper; }

private:
  void create();
  void cleanup();

  Interval* m_root{};
  std::unique_ptr<ossia::net::device_base> m_localDevice;
  Protocols::LocalDevice m_localDeviceWrapper;
};
}
