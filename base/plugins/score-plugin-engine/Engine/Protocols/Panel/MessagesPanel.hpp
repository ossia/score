#pragma once
#include <score/plugins/panel/PanelDelegate.hpp>
#include <score/plugins/panel/PanelDelegateFactory.hpp>
class QListWidget;
class QListView;
namespace Device
{
class DeviceList;
}
namespace Engine
{
class LogMessagesItemModel;
class MessagesPanelDelegate final : public QObject, public score::PanelDelegate
{
  friend class err_sink;
public:
  MessagesPanelDelegate(const score::GUIApplicationContext& ctx);

  void qtLog(const std::string& str);
private:
  QWidget* widget() override;

  const score::PanelStatus& defaultPanelStatus() const override;

  void on_modelChanged(
      score::MaybeDocument oldm, score::MaybeDocument newm) override;

  void setupConnections(Device::DeviceList&);
  void disableConnections();
  Device::DeviceList* getDeviceList(score::MaybeDocument);

  LogMessagesItemModel* m_itemModel{};
  QListView* m_widget{};

  QMetaObject::Connection m_inbound{}, m_outbound{}, m_error{}, m_visible{};
};

class MessagesPanelDelegateFactory final : public score::PanelDelegateFactory
{
  SCORE_CONCRETE("84a66cbe-aee3-496a-b7f4-0ea0d699deac")

  std::unique_ptr<score::PanelDelegate>
  make(const score::GUIApplicationContext& ctx) override;
};
}
