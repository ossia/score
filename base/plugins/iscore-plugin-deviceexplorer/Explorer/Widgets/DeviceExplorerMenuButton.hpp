#pragma once
#include <QWidget>
class DeviceExplorerModel;
namespace iscore{
struct Address;
}
class DeviceExplorerMenuButton final : public QWidget
{
        Q_OBJECT
    public:
        DeviceExplorerMenuButton(DeviceExplorerModel* model);

    signals:
        void addressChosen(const iscore::Address&);
};
