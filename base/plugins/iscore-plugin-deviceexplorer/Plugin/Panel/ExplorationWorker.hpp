#pragma once
#include <QObject>
class DeviceInterface;
#include <DeviceExplorer/Node/Node.hpp>

class ExplorationWorker : public QObject
{
        Q_OBJECT
    public:
        DeviceInterface& dev;
        iscore::Node node; // Result

        ExplorationWorker(DeviceInterface& dev);

    signals:
        void finished();
        void failed(QString);
};
