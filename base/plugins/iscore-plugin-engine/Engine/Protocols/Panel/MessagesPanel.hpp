#pragma once
#include <iscore/plugins/panel/PanelDelegate.hpp>
#include <iscore/plugins/panel/PanelDelegateFactory.hpp>
class QListWidget;
class QListView;
namespace Device
{
class DeviceList;
}
namespace Engine
{
class LogMessagesItemModel;
class PanelDelegate final : public QObject, public iscore::PanelDelegate
{
public:
  PanelDelegate(const iscore::ApplicationContext& ctx);

private:
  QWidget* widget() override;

  const iscore::PanelStatus& defaultPanelStatus() const override;

  void on_modelChanged(
      iscore::MaybeDocument oldm, iscore::MaybeDocument newm) override;

  void setupConnections(Device::DeviceList&);
  Device::DeviceList* getDeviceList(iscore::MaybeDocument);

  LogMessagesItemModel* m_itemModel{};
  QListView* m_widget{};

  QMetaObject::Connection m_inbound, m_outbound, m_error, m_visible;
};

class PanelDelegateFactory final : public iscore::PanelDelegateFactory
{
  ISCORE_CONCRETE("84a66cbe-aee3-496a-b7f4-0ea0d699deac")

  std::unique_ptr<iscore::PanelDelegate>
  make(const iscore::ApplicationContext& ctx) override;
};
}
