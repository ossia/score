#pragma once
//#include <QDialog>
//#include <State/Value.hpp>
//#include <iscore/widgets/WidgetWrapper.hpp>

//class QWidget;
//namespace State
//{
//struct AddressAccessor;
//struct Message;
//class ValueWidget;
//}

//class QComboBox;
//class QFormLayout;

//namespace Explorer
//{
//class AddressAccessorEditWidget;
//class DeviceExplorerModel;

///**
// * @brief The MessageEditDialog class
// *
// * A dialog used to edit a single message.
// * The edited address and value can be found in the respective methods
// * after edition, if the dialog was accepted.
// *
// * A device explorer model is used for completion of the address.
// */
//class MessageEditDialog final : public QDialog
//{
//public:
//  MessageEditDialog(
//      const State::Message& mess, DeviceExplorerModel* model, QWidget* parent);

//  const State::AddressAccessor& address() const;

//  ossia::value value() const;

//private:
//  void initTypeCombo();
//  void on_typeChanged(int t);

//  const State::Message& m_message;

//  AddressAccessorEditWidget* m_addr{};

//  QFormLayout* m_lay{};
//  QComboBox* m_typeCombo{};
//  WidgetWrapper<ValueWidget>* m_val{};
//};
//}
