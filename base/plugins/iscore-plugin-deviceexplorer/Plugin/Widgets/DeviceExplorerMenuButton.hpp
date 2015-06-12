#pragma once
#include <QWidget>
#include <State/Message.hpp>
class DeviceExplorerModel;
class DeviceExplorerMenuButton : public QWidget
{
        Q_OBJECT
    public:
        DeviceExplorerMenuButton(DeviceExplorerModel* model);

    signals:
        void addressChosen(const Address&);
};
