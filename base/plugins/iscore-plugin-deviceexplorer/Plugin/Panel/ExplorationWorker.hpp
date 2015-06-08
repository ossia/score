#pragma once
#include <QObject>
class DeviceInterface;
#include <DeviceExplorer/Node/Node.hpp>

class ExplorationWorker : public QObject
{
        Q_OBJECT
        DeviceInterface& m_dev;
    public:
        Node node;
        ExplorationWorker(DeviceInterface& dev);

        void process();

    signals:
        void finished();
};
