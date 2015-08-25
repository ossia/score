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

        const iscore::Address& address() const
        { return m_address; }

    signals:
        void addressChanged(const iscore::Address&);

    private:
        AddressLineEdit* m_lineEdit{};
        iscore::Address m_address;

};
