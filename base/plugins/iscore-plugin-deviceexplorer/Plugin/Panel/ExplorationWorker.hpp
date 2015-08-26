#pragma once
#include <QObject>
class DeviceInterface;
#include <DeviceExplorer/Node/Node.hpp>

/**
 * @brief The ExplorationWorker class
 *
 * Used as a thread worker to perform refreshing of a remote device without GUI
 * interruption. See DeviceExplorerWidget::refresh() for usage.
 */
class ExplorationWorker : public QObject
{
        Q_OBJECT
    public:
        DeviceInterface& dev;
        iscore::Node node; // Result

        explicit ExplorationWorker(DeviceInterface& dev);

    signals:
        void finished();
        void failed(QString);
};
