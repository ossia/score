#pragma once

#include <QAbstractItemModel>
#include <QGroupBox>
#include <QWidget>
#include <score/command/Dispatchers/CommandDispatcher.hpp>
#include <memory>

#include <Device/Node/DeviceNode.hpp>
#include <Explorer/Explorer/ListeningManager.hpp>
#include <score/model/tree/TreePath.hpp>

class QAction;
class QContextMenuEvent;
class QProgressIndicator;
class QStackedLayout;

class QComboBox;
class QLineEdit;
class QTableView;
namespace Device
{
class ProtocolFactoryList;
}

namespace Explorer
{
class AddressItemModel;
class ListeningHandler;
class DeviceEditDialog;
class DeviceExplorerFilterProxyModel;
class DeviceExplorerModel;
class DeviceExplorerView;
class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT DeviceExplorerWidget final : public QWidget
{
  Q_OBJECT

public:
  explicit DeviceExplorerWidget(
      const Device::ProtocolFactoryList&, QWidget* parent);

  void setModel(DeviceExplorerModel* model);
  DeviceExplorerModel* model() const;
  DeviceExplorerView* view() const;

  // Will block the GUI when refreshing.
  void blockGUI(bool);

  QModelIndex sourceIndex(QModelIndex) const;
  QModelIndex proxyIndex(QModelIndex) const;

  ListeningManager& listeningManager() const
  {
    return *m_listeningManager;
  }

private:
  // User commands
  void edit();
  void refresh();
  void refreshValue();

  void disconnect();
  void reconnect();

  void addAddress(InsertMode insertType);
  void addDevice();
  void exportDevice();
  void addChild();
  void addSibling();

  void removeNodes();
  void learn();
  void findUsage();

  // Answer to user interaction
  void filterChanged();

  void updateActions();
  void updateAddressView();

  // Utilities
  DeviceExplorerFilterProxyModel* proxyModel();

  void buildGUI();
  void populateColumnCBox();

  void contextMenuEvent(QContextMenuEvent* event) override;

  const Device::ProtocolFactoryList& m_protocolList;

  DeviceExplorerView* m_ntView{};
  DeviceExplorerFilterProxyModel* m_proxyModel{};
  DeviceEditDialog* m_deviceDialog{};
  AddressItemModel* m_addressModel{};
  QTableView* m_addressView{};

  QAction* m_disconnect{};
  QAction* m_reconnect{};

  QAction* m_editAction{};
  QAction* m_refreshAction{};
  QAction* m_refreshValueAction{};
  QAction* m_addDeviceAction{};
  QAction* m_addSiblingAction{};
  QAction* m_addChildAction{};
  QAction* m_exportDeviceAction{};

  QAction* m_removeNodeAction{};
  QAction* m_learnAction{};
  QAction* m_findUsageAction{};

  QComboBox* m_columnCBox{};
  QLineEdit* m_nameLEdit{};

  std::unique_ptr<CommandDispatcher<>> m_cmdDispatcher;

  QProgressIndicator* m_refreshIndicator{};
  QStackedLayout* m_lay{};

  std::unique_ptr<ListeningManager> m_listeningManager;
  QMetaObject::Connection m_modelCon;

  QMetaObject::Connection m_addressCon;

Q_SIGNALS:
  void findAddresses(QStringList strlst);
};
}
