#pragma once
#include <QWidget>
class DeviceExplorerModel;
struct Address;
class DeviceExplorerMenuButton : public QWidget
{
        Q_OBJECT
    public:
        DeviceExplorerMenuButton(DeviceExplorerModel* model);

    signals:
        void addressChosen(const Address&);
};
