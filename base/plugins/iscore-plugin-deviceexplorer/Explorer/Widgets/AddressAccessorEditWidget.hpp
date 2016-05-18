#pragma once
#include <State/Address.hpp>
#include <QWidget>
#include <iscore_plugin_deviceexplorer_export.h>

namespace State
{
class AddressAccessorLineEdit;
}

namespace Explorer
{
class DeviceExplorerModel;
class ISCORE_PLUGIN_DEVICEEXPLORER_EXPORT AddressAccessorEditWidget final : public QWidget
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

        State::AddressAccessorLineEdit* m_lineEdit{};
        State::AddressAccessor m_address;
        DeviceExplorerModel* m_model;
};
}
