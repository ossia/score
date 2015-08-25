#pragma once
#include <QtWidgets>
class AddressLineEdit;
class DeviceExplorerModel;
#include <State/Address.hpp>
class AddressEditWidget : public QWidget
{
        Q_OBJECT
    public:
        AddressEditWidget(DeviceExplorerModel* model, QWidget* parent);

        void setAddress(const iscore::Address& addr);

    signals:
        void addressChanged(const iscore::Address&);

    private:
        AddressLineEdit* m_lineEdit{};

};
