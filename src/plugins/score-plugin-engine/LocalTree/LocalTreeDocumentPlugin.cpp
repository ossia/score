// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com
#include "LocalTreeDocumentPlugin.hpp"

#include <Explorer/DocumentPlugin/DeviceDocumentPlugin.hpp>
#include <Explorer/Settings/ExplorerModel.hpp>
#include <LocalTree/Device/LocalProtocolFactory.hpp>
#include <LocalTree/Device/LocalSpecificSettings.hpp>
#include <LocalTree/IntervalComponent.hpp>

#include <score/tools/Bind.hpp>
#include <score/tools/IdentifierGeneration.hpp>

#include <core/document/Document.hpp>
#include <core/document/DocumentModel.hpp>

#include <ossia/network/generic/generic_device.hpp>
#include <ossia/network/local/local.hpp>

#include <Scenario/Document/ScenarioDocument/ScenarioDocumentModel.hpp>

namespace LocalTree
{
Device::DeviceSettings defaultSettings(const score::DocumentContext& ctx)
{
  Device::DeviceSettings s;
  s.protocol = Protocols::LocalProtocolFactory::static_concreteKey();
  s.name = QString("score (%1)").arg(ctx.document.metadata().documentName());
  Protocols::LocalSpecificSettings specif;
  specif.oscPort = 6666;
  specif.wsPort = 9999;
  s.deviceSpecificSettings = QVariant::fromValue(specif);
  return s;
}
}

LocalTree::DocumentPlugin::DocumentPlugin(
    const score::DocumentContext& ctx,
    QObject* parent)
    : score::
        DocumentPlugin{ctx, "LocalTree::DocumentPlugin", parent}
    , m_localDevice{std::make_unique<ossia::net::generic_device>(
          std::make_unique<ossia::net::multiplex_protocol>(),
          "score")}
    , m_localDeviceWrapper{
          *m_localDevice,
          ctx,
          defaultSettings(ctx)}
{
}

LocalTree::DocumentPlugin::~DocumentPlugin()
{
  cleanup();

  auto docplug = context().findPlugin<Explorer::DeviceDocumentPlugin>();
  if (docplug)
    docplug->list().setLocalDevice(nullptr);
}

void LocalTree::DocumentPlugin::init()
{
  m_localDeviceWrapper.init();

  auto& set = m_context.app.settings<Explorer::Settings::Model>();
  if (set.getLocalTree())
  {
    create();
  }

  con(
      set,
      &Explorer::Settings::Model::LocalTreeChanged,
      this,
      [=](bool b) {
        if (b)
          create();
        else
          cleanup();
      },
      Qt::QueuedConnection);

  auto docplug = context().findPlugin<Explorer::DeviceDocumentPlugin>();
  if (docplug)
    docplug->list().setLocalDevice(&m_localDeviceWrapper);
}

void LocalTree::DocumentPlugin::on_documentClosing()
{
  cleanup();
}

void LocalTree::DocumentPlugin::create()
{
  if (m_root)
    cleanup();

  auto& doc = m_context.document.model().modelDelegate();
  auto scenar = dynamic_cast<Scenario::ScenarioDocumentModel*>(&doc);
  if (!scenar)
    return;

  auto& cstr = scenar->baseInterval();
  m_root = new Interval(
      m_localDevice->get_root_node(),
      cstr,
      context(),
      this);
  cstr.components().push_back(m_root);
}

void LocalTree::DocumentPlugin::cleanup()
{
  if (!m_root)
    return;

  m_root->interval().components().remove(m_root);
  m_root = nullptr;
}
