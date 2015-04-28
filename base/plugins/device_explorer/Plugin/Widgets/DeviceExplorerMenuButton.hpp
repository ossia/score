#pragma once
#include <QWidget>
class DeviceExplorerModel;
class DeviceExplorerMenuButton : public QWidget
{
        Q_OBJECT
    public:
        DeviceExplorerMenuButton(DeviceExplorerModel* model);

    signals:
        void addressChosen(QString);
};
