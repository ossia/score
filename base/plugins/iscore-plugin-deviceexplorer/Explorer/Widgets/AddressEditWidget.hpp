#pragma once
#include <State/Address.hpp>
#include <QWidget>
#include <iscore_lib_device_export.h>

class AddressLineEdit;
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

        void setAddress(const iscore::Address& addr);

        const iscore::Address& address() const
        { return m_address; }

    signals:
        void addressChanged(const iscore::Address&);

    private:
        AddressLineEdit* m_lineEdit{};
        iscore::Address m_address;
};
