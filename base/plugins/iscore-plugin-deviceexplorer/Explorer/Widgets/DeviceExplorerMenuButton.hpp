#pragma once
#include <QWidget>


namespace State{
struct Address;
}


namespace DeviceExplorer
{
class DeviceExplorerModel;
class DeviceExplorerMenuButton final : public QWidget
{
        Q_OBJECT
    public:
        DeviceExplorerMenuButton(DeviceExplorerModel* model);

    signals:
        void addressChosen(const State::Address&);
};
}
