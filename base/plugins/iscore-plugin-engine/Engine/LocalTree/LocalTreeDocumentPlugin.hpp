#pragma once
#include <iscore/plugins/documentdelegate/plugin/DocumentPlugin.hpp>
#include <iscore/tools/Metadata.hpp>

#include <Engine/Protocols/Local/LocalDevice.hpp>

#include "GetProperty.hpp"
#include "Property.hpp"
#include "SetProperty.hpp"

namespace iscore
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
class ISCORE_PLUGIN_ENGINE_EXPORT DocumentPlugin
    : public iscore::DocumentPlugin
{
public:
  DocumentPlugin(
      const iscore::DocumentContext& doc, Id<iscore::DocumentPlugin> id, QObject* parent);

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
