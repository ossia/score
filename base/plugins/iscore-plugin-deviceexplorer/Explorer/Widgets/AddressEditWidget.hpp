#pragma once
#include <State/Address.hpp>
#include <QWidget>
#include <iscore_lib_device_export.h>

class AddressLineEdit;
class AddressAccessorLineEdit;


namespace Explorer
{
class DeviceExplorerModel;

/**
 * @brief The AddressEditWidget class
 *
 * Allows editing of an Address.
 * A device explorer model is used for completion.
 *
 */

class ISCORE_LIB_DEVICE_EXPORT AddressEditWidget final : public QWidget
{
        Q_OBJECT
    public:
        AddressEditWidget(DeviceExplorerModel* model, QWidget* parent);

        void setAddress(const State::Address& addr);
        void setAddressString(const QString);

        const State::Address& address() const
        { return m_address; }

        QString addressString() const
        { return m_address.toString(); }


    signals:
        void addressChanged(const State::Address&);

    private:
        void customContextMenuEvent(const QPoint& p);
        void dropEvent(QDropEvent*) override;

        AddressLineEdit* m_lineEdit{};
        State::Address m_address;
        DeviceExplorerModel* m_model;
};





class ISCORE_LIB_DEVICE_EXPORT AddressAccessorEditWidget final : public QWidget
{
        Q_OBJECT
    public:
        AddressAccessorEditWidget(DeviceExplorerModel* model, QWidget* parent);

        void setAddress(const State::Address& addr);
        void setAddress(const State::AddressAccessor& addr);

        const State::AddressAccessor& address() const
        { return m_address; }

        QString addressString() const
        { return m_address.toString(); }

    signals:
        void addressChanged(const State::AddressAccessor&);

    private:
        void customContextMenuEvent(const QPoint& p);
        void dropEvent(QDropEvent*) override;

        AddressAccessorLineEdit* m_lineEdit{};
        State::AddressAccessor m_address;
        DeviceExplorerModel* m_model;
};
}
