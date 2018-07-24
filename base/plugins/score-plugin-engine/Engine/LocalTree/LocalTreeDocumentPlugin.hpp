#pragma once
#include "GetProperty.hpp"
#include "Property.hpp"
#include "SetProperty.hpp"

#include <Engine/Protocols/Local/LocalDevice.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <score/tools/Metadata.hpp>
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
namespace Engine
{
namespace LocalTree
{
class Interval;
class SCORE_PLUGIN_ENGINE_EXPORT DocumentPlugin : public score::DocumentPlugin
{
public:
  DocumentPlugin(
      const score::DocumentContext& doc,
      Id<score::DocumentPlugin> id,
      QObject* parent);

  ~DocumentPlugin();

  void init();

  void on_documentClosing() override;
  ossia::net::generic_device& device()
  {
    return *m_localDevice;
  }
  const ossia::net::generic_device& device() const
  {
    return *m_localDevice;
  }

  Network::LocalDevice& localDevice()
  {
    return m_localDeviceWrapper;
  }

private:
  void create();
  void cleanup();

  Interval* m_root{};
  std::unique_ptr<ossia::net::generic_device> m_localDevice;
  Network::LocalDevice m_localDeviceWrapper;
};
}
}
