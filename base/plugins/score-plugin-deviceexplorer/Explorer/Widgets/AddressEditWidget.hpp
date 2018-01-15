//#pragma once
//#include <QWidget>
//#include <State/Address.hpp>
//#include <score_plugin_deviceexplorer_export.h>

//class QLineEdit;
//namespace Explorer
//{
//class DeviceExplorerModel;

///**
// * @brief The AddressEditWidget class
// *
// * Allows editing of an Address.
// * A device explorer model is used for completion.
// *
// */
//class SCORE_PLUGIN_DEVICEEXPLORER_EXPORT AddressEditWidget final
//    : public QWidget
//{
//  Q_OBJECT
//public:
//  AddressEditWidget(DeviceExplorerModel& model, QWidget* parent);

//  void setAddress(const State::Address& addr);
//  void setAddressString(QString);

//  const State::Address& address() const
//  {
//    return m_address;
//  }

//  QString addressString() const
//  {
//    return m_address.toString();
//  }

//  void dragEnterEvent(QDragEnterEvent* event) override;
//  void dropEvent(QDropEvent*) override;
//Q_SIGNALS:
//  void addressChanged(const State::Address&);

//private:
//  void customContextMenuEvent(const QPoint& p);

//  QLineEdit* m_lineEdit{};
//  State::Address m_address;
//  DeviceExplorerModel& m_model;
//};
//}
