#pragma once
#include <Device/Node/DeviceNode.hpp>
#include <QObject>
#include <QString>


class DeviceInterface;

namespace DeviceExplorer
{
/**
 * @brief The ExplorationWorker class
 *
 * Used as a thread worker to perform refreshing of a remote device without GUI
 * interruption. See DeviceExplorerWidget::refresh() for usage.
 */
class ExplorationWorker final : public QObject
{
        Q_OBJECT
    public:
        DeviceInterface& dev;
        Device::Node node; // Result

        explicit ExplorationWorker(DeviceInterface& dev);

    signals:
        void finished();
        void failed(QString);
};
}
