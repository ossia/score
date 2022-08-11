#pragma once
#include <score/plugins/application/GUIApplicationPlugin.hpp>
#include <score/plugins/documentdelegate/plugin/DocumentPlugin.hpp>

namespace score
{
class MessagesPanelDelegate;
}
namespace Device
{
class DeviceList;
}
namespace Explorer
{
class ApplicationPlugin final : public score::GUIApplicationPlugin
{
public:
  ApplicationPlugin(const score::GUIApplicationContext& app);

private:
  void disableConnections()
  {
    QObject::disconnect(m_inbound);
    QObject::disconnect(m_outbound);
    QObject::disconnect(m_error);
  }
  void setupConnections(score::MessagesPanelDelegate&, Device::DeviceList& devices);
  void on_newDocument(score::Document& doc) override;
  void on_documentChanged(score::Document* olddoc, score::Document* newdoc) override;

  QMetaObject::Connection m_inbound{}, m_outbound{}, m_error{}, m_visible{};
};
}
