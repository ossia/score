#include <ossia/network/base/device.hpp>
#include <Engine/ApplicationPlugin.hpp>
#include <QString>
#include <QVariant>
#include <iscore/document/DocumentContext.hpp>
#include <memory>

#include "LocalDevice.hpp"
#include <ossia/editor/state/state_element.hpp>
#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/local/local.hpp>
#include <ossia/network/minuit/minuit.hpp>
#include <Device/Protocol/DeviceSettings.hpp>
#include <Engine/LocalTree/LocalTreeDocumentPlugin.hpp>
#include <Engine/OSSIA2iscore.hpp>
#include <Engine/Protocols/Local/LocalSpecificSettings.hpp>
#include <Engine/iscore2OSSIA.hpp>
#include <Scenario/Application/ScenarioActions.hpp>
#include <iscore/actions/ActionManager.hpp>
namespace Engine
{
namespace Network
{
LocalDevice::LocalDevice(
    ossia::net::generic_device& dev,
    const iscore::DocumentContext& ctx,
    const Device::DeviceSettings& settings)
    : OSSIADevice{settings}, m_dev{dev}
{
  m_capas.canRefreshTree = true;
  m_capas.canAddNode = false;
  m_capas.canRemoveNode = false;
  m_capas.canSerialize = false;

  auto& appplug
      = ctx.app.applicationPlugin<Engine::ApplicationPlugin>();

  auto& proto = safe_cast<ossia::net::local_protocol&>(dev.getProtocol());

  m_proto = &proto;
  setLogging_impl(isLogging());

  auto& root = dev.getRootNode();

  {
    auto local_play_node = root.createChild("play");
    auto local_play_address
        = local_play_node->createAddress(ossia::val_type::BOOL);
    local_play_address->setValue(bool{false});
    local_play_address->add_callback([&](const ossia::value& v) {
      if (auto val = v.target<bool>())
      {
        if (!appplug.playing() && *val)
        {
          // not playing, play requested
          auto& play_action = appplug.context.actions.action<Actions::Play>();
          play_action.action()->trigger();
        }
        else if (appplug.playing())
        {
          if (appplug.paused() == *val)
          {
            // paused, play requested
            // or playing, pause requested

            auto& play_action
                = appplug.context.actions.action<Actions::Play>();
            play_action.action()->trigger();
          }
        }
      }
    });
  }
  {
    auto local_stop_node = root.createChild("stop");
    auto local_stop_address
        = local_stop_node->createAddress(ossia::val_type::IMPULSE);
    local_stop_address->setValue(ossia::impulse{});
    local_stop_address->add_callback([&](const ossia::value&) {
      auto& stop_action = appplug.context.actions.action<Actions::Stop>();
      stop_action.action()->trigger();
    });
  }

  setRemoteSettings(settings);

  dev.onNodeCreated.connect<LocalDevice, &LocalDevice::nodeCreated>(this);
  dev.onNodeRemoving.connect<LocalDevice, &LocalDevice::nodeRemoving>(this);
  dev.onNodeRenamed.connect<LocalDevice, &LocalDevice::nodeRenamed>(this);
  dev.onAddressCreated.connect<LocalDevice, &LocalDevice::addressCreated>(
      this);
  dev.onAddressModified.connect<LocalDevice, &LocalDevice::addressUpdated>(
      this);
}

LocalDevice::~LocalDevice()
{
}

void LocalDevice::setRemoteSettings(const Device::DeviceSettings& settings)
{
  if (!m_proto)
    return;

  try
  {
    auto set = settings.deviceSpecificSettings.value<LocalSpecificSettings>();
    if (set.remoteName == "")
    {
      set.remoteName = "i-score-remote";
      set.host = "127.0.0.1";
      set.remotePort = 9999;
      set.localPort = 6666;
    }
    m_proto->exposeTo(std::make_unique<ossia::net::minuit_protocol>(
        set.remoteName.toStdString(),
        set.host.toStdString(),
        set.remotePort,
        set.localPort));
  }
  catch (...)
  {
    qDebug() << "LocalDevice: could not expose i-score-remote on port 6666";
  }
}

void LocalDevice::disconnect()
{
  // TODO handle listening ??
  setLogging_impl(false);
}

bool LocalDevice::reconnect()
{
  m_callbacks.clear();
  setRemoteSettings(settings());
  return connected();
}

Device::Node LocalDevice::refresh()
{
  Device::Node iscore_device{settings(), nullptr};

  // Recurse on the children
  auto& ossia_children = m_dev.getRootNode().children();
  iscore_device.reserve(ossia_children.size());
  for (const auto& node : ossia_children)
  {
    iscore_device.push_back(
        Engine::ossia_to_iscore::ToDeviceExplorer(*node.get()));
  }

  iscore_device.get<Device::DeviceSettings>().name
      = QString::fromStdString(m_dev.getName());

  return iscore_device;
}
}
}
